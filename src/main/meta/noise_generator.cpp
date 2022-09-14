/*
 * Copyright (C) 2022 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2022 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 27 Feb 2022
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/noise_generator.h>

#define LSP_PLUGINS_NOISE_GENERATOR_VERSION_MAJOR       1
#define LSP_PLUGINS_NOISE_GENERATOR_VERSION_MINOR       0
#define LSP_PLUGINS_NOISE_GENERATOR_VERSION_MICRO       0

#define LSP_PLUGINS_NOISE_GENERATOR_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_NOISE_GENERATOR_VERSION_MAJOR, \
        LSP_PLUGINS_NOISE_GENERATOR_VERSION_MINOR, \
        LSP_PLUGINS_NOISE_GENERATOR_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers

        static const port_item_t noise_lcg_dist[] =
        {
            { "Uniform",                        "noise_generator.lcg.uniform" },
            { "Exponential",                    "noise_generator.lcg.exponential" },
            { "Triangular",                     "noise_generator.lcg.triangular" },
            { "Gaussian",                       "noise_generator.lcg.gaussian" },
            { NULL,                             NULL }
        };

        static const port_item_t noise_velvet_type[] =
        {
            { "OVN",                            "noise_generator.velvet.ovn" },
            { "OVNA",                           "noise_generator.velvet.ovna" },
            { "ARN",                            "noise_generator.velvet.arn" },
            { "TRN",                            "noise_generator.velvet.trn" },
            { NULL,                             NULL }
        };

        static const port_item_t noise_type[] =
        {
            { "MLS",                            "noise_generator.type.mls" },
            { "LCG",                            "noise_generator.type.lcg" },
            { "VELVET",                         "noise_generator.type.velvet" },
            { NULL,                             NULL }
        };

        static const port_item_t noise_color[] =
        {
            { "White",                          "noise_generator.color.white" },
            { "Pink",                           "noise_generator.color.pink" },
            { "Red",                            "noise_generator.color.red" },
            { "Blue",                           "noise_generator.color.blue" },
            { "Violet",                         "noise_generator.color.violet" },
            { "Arbitrary (Neper per Neper)",    "noise_generator.color.npn" },
            { "Arbitrary (dB per Octave)",      "noise_generator.color.dbo" },
            { "Arbitrary (dB per Decade)",      "noise_generator.color.dbd" },
            { NULL,                             NULL }
        };

        static const port_item_t channel_mode[] =
        {
            { "Overwrite",                      "noise_generator.mode.over" },
            { "Add",                            "noise_generator.mode.add" },
            { "Multiply",                       "noise_generator.mode.mult" },
            { NULL,                             NULL }
        };

        #define CHANNEL_AUDIO_PORTS(id, label) \
            AUDIO_INPUT("in" id, "Input" label), \
            AUDIO_OUTPUT("out" id, "Output" label)

        #define CHANNEL_SWITCHES(id, label) \
            SWITCH("chsl" id, "Solo Switch" label, 0.0f), \
            SWITCH("chmt" id, "Mute Switch" label, 0.0f)

        #define LCG_CONTROLS(id, label) \
            COMBO("ld" id, "LCG Distribution" label, noise_generator::NOISE_LCG_DFL, noise_lcg_dist)

        #define VELVET_CONTROLS(id, label) \
            COMBO("vt" id, "Velvet Type" label, noise_generator::NOISE_VELVET_DFL, noise_velvet_type), \
            LOG_CONTROL("vw" id, "Velvet Window" label, U_SEC, noise_generator::VELVET_WINDOW_DURATION), \
            LOG_CONTROL("vad" id, "Velvet ARN Delta" label, U_NONE, noise_generator::VELVET_ARN_DELTA), \
            SWITCH("vc" id, "Velvet Crushing", 0.0f), \
            CONTROL("vcp" id, "Velvet Crushing Probability" label, U_PERCENT, noise_generator::VELVET_CRUSH_PROB)

        #define COLOR_CONTROLS(id, label) \
            COMBO("cs" id, "Color Selector" label, noise_generator::NOISE_COLOR_DFL, noise_color), \
            CONTROL("csn" id, "Color Slope NPN" label, U_NONE, noise_generator::NOISE_COLOR_SLOPE_NPN), \
            CONTROL("cso" id, "Color Slope dBO" label, U_DB, noise_generator::NOISE_COLOR_SLOPE_DBO), \
            CONTROL("csd" id, "Color Slope dBD" label, U_DB, noise_generator::NOISE_COLOR_SLOPE_DBD)

        #define NOISE_CONTROLS(id, label, noise_t) \
            COMBO("nt" id, "Noise Type" label, noise_t, noise_type), \
            AMP_GAIN100("na" id, "Noise Amplitude", noise_generator::NOISE_AMPLITUDE_DFL), \
            CONTROL("no" id, "Noise Offset" label, U_NONE, noise_generator::NOISE_OFFSET), \
            SWITCH("ns" id, "Noise Solo" label, 0.0f), \
            SWITCH("nm" id, "Noise Mute" label, 0.0f), \
            SWITCH("ni" id, "Noise Inaudible", 0.0f)

        #define GENERATOR_CONTROLS(id, label, noise_t) \
            NOISE_CONTROLS(id, label, noise_t), \
            LCG_CONTROLS(id, label), \
            VELVET_CONTROLS(id, label), \
            COLOR_CONTROLS(id, label), \
            METER_GAIN("nlm" id, "Noise Level Meter", GAIN_AMP_P_24_DB), \
            MESH("nsg" id, "Noise Spectrum Graph", 2, noise_generator::MESH_POINTS + 4)

        #define CHANNEL_CONTROLS(id, label, g1, g2, g3, g4) \
            COMBO("chm" id, "Channel Mode" label, noise_generator::CHANNEL_MODE_DFL, channel_mode), \
            AMP_GAIN100("gg1" id, "Generator 1 Gain" label, g1), \
            AMP_GAIN100("gg2" id, "Generator 2 Gain" label, g2), \
            AMP_GAIN100("gg3" id, "Generator 3 Gain" label, g3), \
            AMP_GAIN100("gg4" id, "Generator 4 Gain" label, g4), \
            AMP_GAIN100("gout" id, "Output gain" label, GAIN_AMP_0_DB), \
            METER_GAIN("ilm" id, "Input Level Meter" label, GAIN_AMP_P_24_DB), \
            METER_GAIN("olm" id, "Output Level Meter" label, GAIN_AMP_P_24_DB)

        #define MCHANNEL_CONTROLS(id, label, g1, g2, g3, g4) \
            SWITCH("chs" id, "Channel Solo" label, 0.0f), \
            SWITCH("chm" id, "Channel Mute" label, 0.0f), \
            CHANNEL_CONTROLS(id, label, g1, g2, g3, g4)

        static const port_t noise_generator_x1_ports[] =
        {
            CHANNEL_AUDIO_PORTS("_1", " 1"),
            BYPASS,

            GENERATOR_CONTROLS("_1", " 1", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_2", " 2", noise_generator::NOISE_TYPE_OFF),
            GENERATOR_CONTROLS("_3", " 3", noise_generator::NOISE_TYPE_OFF),
            GENERATOR_CONTROLS("_4", " 4", noise_generator::NOISE_TYPE_OFF),

            CHANNEL_CONTROLS("_1", " 1", 1.0f, 0.0f, 0.0f, 0.0f),

            PORTS_END
        };

        static const port_t noise_generator_x2_ports[] =
        {
            CHANNEL_AUDIO_PORTS("_1", " 1"),
            CHANNEL_AUDIO_PORTS("_2", " 2"),
            BYPASS,

            GENERATOR_CONTROLS("_1", " 1", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_2", " 2", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_3", " 3", noise_generator::NOISE_TYPE_OFF),
            GENERATOR_CONTROLS("_4", " 4", noise_generator::NOISE_TYPE_OFF),

            MCHANNEL_CONTROLS("_1", " 1", 1.0f, 0.0f, 0.0f, 0.0f),
            MCHANNEL_CONTROLS("_2", " 2", 0.0f, 1.0f, 0.0f, 0.0f),

            PORTS_END
        };

        static const port_t noise_generator_x4_ports[] =
        {
            CHANNEL_AUDIO_PORTS("_1", " 1"),
            CHANNEL_AUDIO_PORTS("_2", " 2"),
            CHANNEL_AUDIO_PORTS("_3", " 3"),
            CHANNEL_AUDIO_PORTS("_4", " 4"),
            BYPASS,

            GENERATOR_CONTROLS("_1", " 1", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_2", " 2", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_3", " 3", noise_generator::NOISE_TYPE_DFL),
            GENERATOR_CONTROLS("_4", " 4", noise_generator::NOISE_TYPE_DFL),

            MCHANNEL_CONTROLS("_1", " 1", 1.0f, 0.0f, 0.0f, 0.0f),
            MCHANNEL_CONTROLS("_2", " 2", 0.0f, 1.0f, 0.0f, 0.0f),
            MCHANNEL_CONTROLS("_3", " 3", 0.0f, 0.0f, 1.0f, 0.0f),
            MCHANNEL_CONTROLS("_4", " 4", 0.0f, 0.0f, 0.0f, 1.0f),

            PORTS_END
        };

        static const int noise_generator_classes[] = { C_UTILITY, -1};

        const meta::bundle_t noise_generator_bundle =
        {
            "noise_generator",
            "Noise Generator",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "A flexible noise generator supporting different algorithms, colors, and inaudible noise."
        };

        const plugin_t noise_generator_x1 =
        {
            "Noise Generator x1",
            "Noise Generator x1",
            "NG1",
            &developers::s_tronci,
            "noise_generator_x1",
            LSP_LV2_URI("noise_generator_x1"),
            LSP_LV2UI_URI("noise_generator_x1"),
            "lng0",
            LSP_LADSPA_NOISE_GENERATOR_BASE + 0,
            LSP_LADSPA_URI("noise_generator_x1"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            noise_generator_x1_ports,
            "util/noise_generator/x1.xml",
            NULL,
            NULL,
            &noise_generator_bundle
        };

        const plugin_t noise_generator_x2 =
        {
            "Noise Generator x2",
            "Noise Generator x2",
            "NG2",
            &developers::s_tronci,
            "noise_generator_x2",
            LSP_LV2_URI("noise_generator_x2"),
            LSP_LV2UI_URI("noise_generator_x2"),
            "lng1",
            LSP_LADSPA_NOISE_GENERATOR_BASE + 1,
            LSP_LADSPA_URI("noise_generator_x2"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            noise_generator_x2_ports,
            "util/noise_generator/x2.xml",
            NULL,
            NULL,
            &noise_generator_bundle
        };

        const plugin_t noise_generator_x4 =
        {
            "Noise Generator x4",
            "Noise Generator x4",
            "NG4",
            &developers::s_tronci,
            "noise_generator_x4",
            LSP_LV2_URI("noise_generator_x4"),
            LSP_LV2UI_URI("noise_generator_x4"),
            "lng2",
            LSP_LADSPA_NOISE_GENERATOR_BASE + 2,
            LSP_LADSPA_URI("noise_generator_x4"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            noise_generator_x4_ports,
            "util/noise_generator/x4.xml",
            NULL,
            NULL,
            &noise_generator_bundle
        };

    } /* namespace meta */
} /* namespace lsp */
