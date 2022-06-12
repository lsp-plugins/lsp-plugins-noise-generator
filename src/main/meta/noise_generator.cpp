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
            {"Uniform",        					"noise_generator.lcg.uniform"},
            {"Exponential",						"noise_generator.lcg.exponential"},
            {"Triangular",     					"noise_generator.lcg.triangular"},
            {"Gaussian",       					"noise_generator.lcg.gaussian"},
            {NULL,          					NULL}
        };

        static const port_item_t noise_velvet_type[] =
        {
            {"OVN",        						"noise_generator.velvet.ovn"},
            {"OVNA",							"noise_generator.velvet.ovna"},
            {"ARN",     						"noise_generator.velvet.arn"},
            {"TRN",       						"noise_generator.velvet.trn"},
            {NULL,          					NULL}
        };

        static const port_item_t noise_type[] =
        {
            {"MLS",        						"noise_generator.type.mls"},
            {"LCG",								"noise_generator.type.lcg"},
            {"VELVET",     						"noise_generator.type.velvet"},
            {NULL,          					NULL}
        };

        static const port_item_t noise_color[] =
        {
            {"White",        					"noise_generator.color.white"},
            {"Pink",							"noise_generator.color.pink"},
            {"Red",     						"noise_generator.color.red"},
            {"Blue",       						"noise_generator.color.blue"},
			{"Violet",       					"noise_generator.color.violet"},
			{"Arbitrary (Neper per Neper)",		"noise_generator.color.npn"},
			{"Arbitrary (dB per Octave)",   	"noise_generator.color.dbo"},
			{"Arbitrary (dB per Decade)",    	"noise_generator.color.dbd"},
            {NULL,          					NULL}
        };

        static const port_item_t noise_mode[] =
        {
            {"Overwrite",        				"noise_generator.mode.over"},
            {"Add",								"noise_generator.mode.add"},
            {"Multiply",     					"noise_generator.mode.mult"},
            {NULL,          					NULL}
        };

        static const port_item_t ng_channels_x2[] =
        {
            {"1",           NULL },
            {"2",           NULL },
            {NULL,          NULL}
        };

        static const port_item_t ng_channels_x4[] =
        {
            {"1",           NULL },
            {"2",           NULL },
            {"3",           NULL },
            {"4",           NULL },
            {NULL,          NULL}
        };

		#define CHANNEL_SELECTOR(ng_channels) \
			COMBO("ng_cs", "Noise Generator Channel Selector", 0, ng_channels)

		#define CHANNEL_AUDIO_PORTS(id, label) \
			AUDIO_INPUT("in" id, "Input" label), \
			AUDIO_OUTPUT("out" id, "Output" label)

		#define CHANNEL_SWITCHES(id, label) \
			SWITCH("glsw" id, "Global Switch" label, 0.0f), \
			SWITCH("chsl" id, "Solo Switch" label, 0.0f), \
			SWITCH("chmt" id, "Mute Switch" label, 0.0f)

		#define LCG_CONTROLS(id, label) \
			COMBO("lcgd" id, "LCG Distribution" label, noise_generator::NOISE_LCG_DFL, noise_lcg_dist)

		#define VELVET_CONTROLS(id, label) \
			COMBO("velt" id, "Velvet Type" label, noise_generator::NOISE_VELVET_DFL, noise_velvet_type), \
			LOG_CONTROL("velw" id, "Velvet Window" label, U_SEC, noise_generator::VELVET_WINDOW_DURATION), \
			LOG_CONTROL("veld" id, "Velvet ARN Delta" label, U_NONE, noise_generator::VELVET_ARN_DELTA), \
			SWITCH("velcs" id, "Velvet Crushing Switch", 0.0f), \
			CONTROL("velc" id, "Velvet Crushing Probability" label, U_PERCENT, noise_generator::VELVET_CRUSH_PROB)

		#define COLOR_CONTROLS(id, label) \
			COMBO("cols" id, "Color Selector" label, noise_generator::NOISE_COLOR_DFL, noise_color), \
			CONTROL("csnpn" id, "Color Slope NPN" label, U_NONE, noise_generator::NOISE_COLOR_SLOPE_NPN), \
			CONTROL("csdbo" id, "Color Slope DBO" label, U_DB, noise_generator::NOISE_COLOR_SLOPE_DBO), \
			CONTROL("csdbd" id, "Color Slope DBD" label, U_DB, noise_generator::NOISE_COLOR_SLOPE_DBD)

		#define NOISE_CONTROLS(id, label) \
			COMBO("nst" id, "Noise Type" label, noise_generator::NOISE_TYPE_DFL, noise_type), \
			COMBO("nsm" id, "Noise Mode" label, noise_generator::NOISE_MODE_DFL, noise_mode), \
			AMP_GAIN10("nsa" id, "Noise Amplitude", noise_generator::NOISE_AMPLITUDE_DFL), \
			CONTROL("nso" id, "Noise Offset" label, U_NONE, noise_generator::NOISE_OFFSET), \
			SWITCH("inas" id, "Make Inaudible Switch", 0.0f)

		#define CHANNEL_CONTROLS(id, label) \
			LCG_CONTROLS(id, label), \
			VELVET_CONTROLS(id, label), \
			COLOR_CONTROLS(id, label), \
			NOISE_CONTROLS(id, label)

    	static const port_t noise_generator_x1_ports[] =
        {
        	CHANNEL_AUDIO_PORTS("_1", " 1"),
			CHANNEL_CONTROLS("_1", " 1"),

            PORTS_END
        };

    	static const port_t noise_generator_x2_ports[] =
        {
        	CHANNEL_AUDIO_PORTS("_1", " 1"),
			CHANNEL_AUDIO_PORTS("_2", " 2"),

            CHANNEL_SELECTOR(ng_channels_x2),

            CHANNEL_CONTROLS("", " Global"),
            CHANNEL_CONTROLS("_1", " 1"),
            CHANNEL_CONTROLS("_2", " 2"),

            CHANNEL_SWITCHES("_1", " 1"),
            CHANNEL_SWITCHES("_2", " 2"),

            PORTS_END
        };

    	static const port_t noise_generator_x4_ports[] =
        {
        	CHANNEL_AUDIO_PORTS("_1", " 1"),
			CHANNEL_AUDIO_PORTS("_2", " 2"),
        	CHANNEL_AUDIO_PORTS("_3", " 3"),
			CHANNEL_AUDIO_PORTS("_4", " 4"),

            CHANNEL_SELECTOR(ng_channels_x4),

            CHANNEL_CONTROLS("", " Global"),
            CHANNEL_CONTROLS("_1", " 1"),
            CHANNEL_CONTROLS("_2", " 2"),
            CHANNEL_CONTROLS("_3", " 3"),
            CHANNEL_CONTROLS("_4", " 4"),

            CHANNEL_SWITCHES("_1", " 1"),
            CHANNEL_SWITCHES("_2", " 2"),
			CHANNEL_SWITCHES("_3", " 3"),
			CHANNEL_SWITCHES("_4", " 4"),

            PORTS_END
        };

        static const int noise_generator_classes[] = { C_UTILITY, -1};

        const meta::bundle_t noise_generator_bundle =
        {
            "noise_generator",
            "Noise Generator",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
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
            "----",         // TODO: fill valid VST2 ID (4 letters/digits)
            0,              // TODO: fill valid LADSPA identifier (positive decimal integer)
            LSP_LADSPA_URI("noise_generator_x1"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_DUMP_STATE,
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
            "----",         // TODO: fill valid VST2 ID (4 letters/digits)
            0,              // TODO: fill valid LADSPA identifier (positive decimal integer)
            LSP_LADSPA_URI("noise_generator_x2"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_DUMP_STATE,
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
            "----",         // TODO: fill valid VST2 ID (4 letters/digits)
            0,              // TODO: fill valid LADSPA identifier (positive decimal integer)
            LSP_LADSPA_URI("noise_generator_x4"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_DUMP_STATE,
            noise_generator_x4_ports,
            "util/noise_generator/x4.xml",
            NULL,
			NULL,
            &noise_generator_bundle
        };

    } /* namespace meta */
} /* namespace lsp */
