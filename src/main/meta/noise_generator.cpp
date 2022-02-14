/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-noise-generator
 * Created on: 25 нояб. 2020 г.
 *
 * lsp-plugins-noise-generator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-noise-generator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-noise-generator. If not, see <https://www.gnu.org/licenses/>.
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
        static const port_t noise_generator_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            INT_CONTROL("d_in", "Delay in samples", U_SAMPLES, noise_generator::SAMPLES),
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,

            // Output controls
            METER_MINMAX("d_out", "Delay time in milliseconds", U_MSEC, 0.0f, noise_generator::DELAY_OUT_MAX_TIME),
            METER_GAIN("min", "Input gain", GAIN_AMP_P_48_DB),
            METER_GAIN("mout", "Output gain", GAIN_AMP_P_48_DB),

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t noise_generator_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            INT_CONTROL("d_in", "Delay in samples", U_SAMPLES, noise_generator::SAMPLES),
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,

            // Output controls
            METER_MINMAX("d_out", "Delay time in milliseconds", U_MSEC, 0.0f, noise_generator::DELAY_OUT_MAX_TIME),
            METER_GAIN("min_l", "Input gain left",  GAIN_AMP_P_48_DB),
            METER_GAIN("mout_l", "Output gain left",  GAIN_AMP_P_48_DB),
            METER_GAIN("min_r", "Input gain right",  GAIN_AMP_P_48_DB),
            METER_GAIN("mout_r", "Output gain right", GAIN_AMP_P_48_DB),

            PORTS_END
        };

        static const int noise_generator_classes[] = { C_DELAY, -1 };

        const meta::bundle_t noise_generator_bundle =
        {
            "noise_generator",
            "Noise Generator",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
        };

        const plugin_t noise_generator_mono =
        {
            "Pluginschablone Mono",
            "Plugin Template Mono",
            "PS1M",
            &developers::v_sadovnikov,
            "noise_generator_mono",
            LSP_LV2_URI("noise_generator_mono"),
            LSP_LV2UI_URI("noise_generator_mono"),
            "----",         // TODO: fill valid VST2 ID (4 letters/digits)
            0,              // TODO: fill valid LADSPA identifier (positive decimal integer)
            LSP_LADSPA_URI("noise_generator_mono"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_DUMP_STATE,
            noise_generator_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &noise_generator_bundle
        };

        const plugin_t noise_generator_stereo =
        {
            "Pluginschablone Stereo",
            "Plugin Template Stereo",
            "PS1S",
            &developers::v_sadovnikov,
            "noise_generator_stereo",
            LSP_LV2_URI("noise_generator_stereo"),
            LSP_LV2UI_URI("noise_generator_stereo"),
            "----",         // TODO: fill valid VST2 ID (4 lower-case letters/digits)
            0,              // TODO: fill valid LADSPA identifier (positive ecimal integer)
            LSP_LADSPA_URI("noise_generator_stereo"),
            LSP_PLUGINS_NOISE_GENERATOR_VERSION,
            noise_generator_classes,
            E_DUMP_STATE,
            noise_generator_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &noise_generator_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



