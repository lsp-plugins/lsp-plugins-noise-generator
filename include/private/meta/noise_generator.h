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

#ifndef PRIVATE_META_NOISE_GENERATOR_H_
#define PRIVATE_META_NOISE_GENERATOR_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct noise_generator_metadata
        {
            static constexpr float  ZOOM_MIN                    = GAIN_AMP_M_36_DB;
            static constexpr float  ZOOM_MAX                    = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_DFL                    = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_STEP                   = 0.025f;

            static constexpr float  VELVET_WINDOW_DURATION_MIN	= 0.0f;
            static constexpr float  VELVET_WINDOW_DURATION_MAX  = 0.1f;
            static constexpr float  VELVET_WINDOW_DURATION_DFL 	= 0.0f;
            static constexpr float  VELVET_WINDOW_DURATION_STEP	= 1.0e-3f;

            static constexpr float  VELVET_ARN_DELTA_MIN		= 0.0f;
            static constexpr float  VELVET_ARN_DELTA_MAX  		= 1.0f;
            static constexpr float  VELVET_ARN_DELTA_DFL 		= 0.5f;
            static constexpr float  VELVET_ARN_DELTA_STEP		= 1.0e-3f;

            static constexpr float  VELVET_CRUSH_PROB_MIN		= 0.0f;
            static constexpr float  VELVET_CRUSH_PROB_MAX  		= 100.0f;
            static constexpr float  VELVET_CRUSH_PROB_DFL 		= 50.0f;
            static constexpr float  VELVET_CRUSH_PROB_STEP		= 1.0e-2f;

            static constexpr float  NOISE_COLOR_SLOPE_NPN_MIN	= -3.0f;
            static constexpr float  NOISE_COLOR_SLOPE_NPN_MAX	= 3.0f;
            static constexpr float  NOISE_COLOR_SLOPE_NPN_DFL 	= -0.5f;  // Pink
            static constexpr float  NOISE_COLOR_SLOPE_NPN_STEP	= 1.0e-3f;

            static constexpr float  NOISE_COLOR_SLOPE_DBO_MIN	= -18.0f;
            static constexpr float  NOISE_COLOR_SLOPE_DBO_MAX	= 18.0f;
            static constexpr float  NOISE_COLOR_SLOPE_DBO_DFL 	= -3.01f;  // Pink
            static constexpr float  NOISE_COLOR_SLOPE_DBO_STEP	= 0.1f;

            static constexpr float  NOISE_COLOR_SLOPE_DBD_MIN	= -60.0f;
            static constexpr float  NOISE_COLOR_SLOPE_DBD_MAX	= 60.0f;
            static constexpr float  NOISE_COLOR_SLOPE_DBD_DFL 	= -10.0f;  // Pink
            static constexpr float  NOISE_COLOR_SLOPE_DBD_STEP	= 0.1f;

            static constexpr float  IN_GAIN_DFL                 = 1.0f;
            static constexpr float  OUT_GAIN_DFL                = 1.0f;

            static constexpr float  NOISE_AMPLITUDE_DFL			= 1.0f;

            static constexpr float  NOISE_OFFSET_MIN			= -10.0f;
            static constexpr float  NOISE_OFFSET_MAX  			= 10.0f;
            static constexpr float  NOISE_OFFSET_DFL 			= 0.0f;
            static constexpr float  NOISE_OFFSET_STEP			= 0.1f;

            static constexpr size_t NUM_GENERATORS              = 4;
            static constexpr size_t MESH_POINTS                 = 640;

            enum noise_type_selector_t
            {
                NOISE_TYPE_OFF,
                NOISE_TYPE_MLS,
                NOISE_TYPE_LCG,
                NOISE_TYPE_VELVET,

                NOISE_TYPE_DFL  = NOISE_TYPE_LCG
            };

            enum noise_color_selector_t
            {
                NOISE_COLOR_WHITE,
                NOISE_COLOR_PINK,
                NOISE_COLOR_RED,
                NOISE_COLOR_BLUE,
                NOISE_COLOR_VIOLET,
                NOISE_COLOR_ARBITRARY_NPN,
                NOISE_COLOR_ARBITRARY_DBO,
                NOISE_COLOR_ARBITRARY_DBD,

                NOISE_COLOR_DFL = NOISE_COLOR_WHITE
            };

            enum lcg_dist_selector_t
            {
                NOISE_LCG_UNIFORM,
                NOISE_LCG_EXPONENTIAL,
                NOISE_LCG_TRIANGULAR,
                NOISE_LCG_GAUSSIAN,

                NOISE_LCG_DFL = NOISE_LCG_UNIFORM
            };

            enum velvet_type_selector_t
            {
                NOISE_VELVET_OVN,
                NOISE_VELVET_OVNA,
                NOISE_VELVET_ARN,
                NOISE_VELVET_TRN,

                NOISE_VELVET_DFL = NOISE_VELVET_OVN
            };

        	enum noise_mode_selector_t
			{
        		CHANNEL_MODE_OVERWRITE,
        		CHANNEL_MODE_ADD,
        		CHANNEL_MODE_MULT,

        		CHANNEL_MODE_DFL = CHANNEL_MODE_OVERWRITE
			};

        } noise_generator;

        // Plugin type metadata
        extern const plugin_t noise_generator_x1;
        extern const plugin_t noise_generator_x2;
        extern const plugin_t noise_generator_x4;
    }
}

#endif /* PRIVATE_META_NOISE_GENERATOR_H_ */
