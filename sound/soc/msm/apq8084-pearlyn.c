/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/slimbus/slimbus.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm_params.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"

#define DRV_NAME "apq8084-asoc-pearlyn"

#define SAMPLING_RATE_32KHZ   32000
#define SAMPLING_RATE_44DOT1KHZ 44100
#define SAMPLING_RATE_48KHZ 48000
#define SAMPLING_RATE_96KHZ 96000
#define SAMPLING_RATE_128KHZ   128000
#define SAMPLING_RATE_176DOT4KHZ  176400
#define SAMPLING_RATE_192KHZ 192000

static inline int param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);
		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static int msm_hdmi_rx_ch = 2;
static int hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int apq8084_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return 0;
	case SND_SOC_DAPM_POST_PMD:
		return 0;
	}
	return 0;
}

static const struct snd_soc_dapm_widget apq8084_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	apq8084_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};

static char const *hdmi_rx_sample_rate_text[] = {"KHZ_32", "KHZ_44_1", "KHZ_48",
						 "KHZ_96", "KHZ_128",
						 "KHZ_176_4", "KHZ_192"};

enum {
	HDMI_RATE_32KHZ = 0,
	HDMI_RATE_44DOT1KHZ,
	HDMI_RATE_48KHZ,
	HDMI_RATE_96KHZ,
	HDMI_RATE_128KHZ,
	HDMI_RATE_176DOT4KHZ,
	HDMI_RATE_192KHZ
};

static int hdmi_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (hdmi_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int hdmi_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_hdmi_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hdmi_rx_ch  = %d\n", __func__,
			msm_hdmi_rx_ch);
	ucontrol->value.integer.value[0] = msm_hdmi_rx_ch - 2;
	return 0;
}

static int msm_hdmi_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	msm_hdmi_rx_ch = ucontrol->value.integer.value[0] + 2;
	if (msm_hdmi_rx_ch > 8) {
		pr_err("%s: channels exceeded 8.Limiting to max channels-8\n",
			__func__);
		msm_hdmi_rx_ch = 8;
	}
	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__, msm_hdmi_rx_ch);
	return 1;
}

static int hdmi_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (hdmi_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = HDMI_RATE_192KHZ;
		break;
	case SAMPLING_RATE_176DOT4KHZ:
		sample_rate_val = HDMI_RATE_176DOT4KHZ;
		break;
	case SAMPLING_RATE_128KHZ:
		sample_rate_val = HDMI_RATE_128KHZ;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = HDMI_RATE_96KHZ;
		break;
	case SAMPLING_RATE_44DOT1KHZ:
		sample_rate_val = HDMI_RATE_44DOT1KHZ;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = HDMI_RATE_32KHZ;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = HDMI_RATE_48KHZ;
		break;
	}
	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		  hdmi_rx_sample_rate);
	return 0;
}

static int hdmi_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case HDMI_RATE_192KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case HDMI_RATE_176DOT4KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_176DOT4KHZ;
		break;
	case HDMI_RATE_128KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_128KHZ;
		break;
	case HDMI_RATE_96KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case HDMI_RATE_44DOT1KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_44DOT1KHZ;
		break;
	case HDMI_RATE_32KHZ:
		hdmi_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case HDMI_RATE_48KHZ:
	default:
		hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		 hdmi_rx_sample_rate);
	return 0;
}

static int apq8084_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				hdmi_rx_bit_format);
	if (channels->max < 2)
		channels->min = channels->max = 2;
	rate->min = rate->max = hdmi_rx_sample_rate;
	channels->min = channels->max = msm_hdmi_rx_ch;
	return 0;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_sample_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[0],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", msm_snd_enum[1],
			hdmi_rx_bit_format_get, hdmi_rx_bit_format_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", msm_snd_enum[2],
			hdmi_rx_sample_rate_get, hdmi_rx_sample_rate_put),
};

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_info("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));
	rtd->pmdown_time = 0;
	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0)
		return err;

	snd_soc_dapm_new_controls(dapm, apq8084_dapm_widgets,
				ARRAY_SIZE(apq8084_dapm_widgets));

	snd_soc_dapm_sync(dapm);
	return 0;
}

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link apq8084_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "APQ8084 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "APQ8084 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "APQ8084 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "APQ8084 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "APQ8084 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
};

static struct snd_soc_dai_link apq8084_hdmi_dai_link[] = {
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-hdmi-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = apq8084_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.init = msm_audrx_init,
	},
};

static struct snd_soc_dai_link apq8084_pearlyn_dai_links[
		 ARRAY_SIZE(apq8084_common_dai_links) +
		 ARRAY_SIZE(apq8084_hdmi_dai_link)];

struct snd_soc_card snd_soc_card_pearlyn_taiko_apq8084 = {
	.name		= "apq8084-pearlyn-snd-card",
};

static const struct of_device_id apq8084_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8084-audio-pearlyn"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = &snd_soc_card_pearlyn_taiko_apq8084;

	memcpy(apq8084_pearlyn_dai_links,
	       apq8084_common_dai_links,
	       sizeof(apq8084_common_dai_links));

	if (of_property_read_bool(dev->of_node, "qcom,hdmi-audio-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(apq8084_pearlyn_dai_links +
			ARRAY_SIZE(apq8084_common_dai_links),
			apq8084_hdmi_dai_link,
			sizeof(apq8084_hdmi_dai_link));
	} else {
		dev_dbg(dev, "%s(): No hdmi audio support\n", __func__);
	}

	card->dai_link = apq8084_pearlyn_dai_links;
	card->num_links = ARRAY_SIZE(apq8084_pearlyn_dai_links);

	return card;
}

static int apq8084_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}
	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

static int apq8084_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver apq8084_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8084_asoc_machine_of_match,
	},
	.probe = apq8084_asoc_machine_probe,
	.remove = apq8084_asoc_machine_remove,
};
module_platform_driver(apq8084_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8084_asoc_machine_of_match);
