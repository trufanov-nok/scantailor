/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) Joseph Artsimovich <joseph_a@mail.ru>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INI_KEYS_H
#define INI_KEYS_H

#include "config.h"
#include <QSettings> // propagate header

#include "filters/output/DespeckleLevel.h"
#include "filters/publish/DjbzDispatcher.h"
#include "filters/publish/FreeImageFilters.h"
#include "exporting/ExportModes.h"
#include <QSize>

/* App settings */

static const char* _key_app_maximized = "main_window/maximized";
static const bool _key_app_maximized_def = false;
static const char* _key_app_language = "main_window/language";
static const char* _key_app_style = "main_window/style";
static const char* _key_app_stylsheet_dir = "main_window/qss_dir";
#ifdef _WIN32
static const char* _key_app_style_def = "windows";
static const char* _key_app_stylsheet_dir_def = STYLESHEETS_DIR_REL;
#elif defined(Q_OS_OSX)
static const char* _key_app_style_def = "macintosh";
static const char* _key_app_stylsheet_dir_def = STYLESHEETS_DIR_REL;
#else
static const char* _key_app_style_def = "fusion";
static const char* _key_app_stylsheet_dir_def = STYLESHEETS_DIR_ABS;
#endif
static const char* _key_app_stylsheet_file = "main_window/qss_file";
static const char* _key_app_empty_palette = "main_window/palette";
static const bool _key_app_empty_palette_def = false;
static const char* _key_app_open_filetype_filter = "main_window/filetype_filter";
static const char* _key_app_docking_enabled = "docking_panels/enabled";
static const bool _key_app_docking_enabled_def = false;
#ifndef ENABLE_OPENJPEG
static const char* _key_app_open_filetype_filter_def = "*.png *.tiff *.tif *.jpeg *.jpg *.bmp";
#else
static const char* _key_app_open_filetype_filter_def = "*.png *.tiff *.tif *.jpeg *.jpg *.bmp *.jp2";
#endif
static const char* _key_app_alert_cmd = "main_window/external_alarm_cmd";
#if (defined(Q_OS_UNIX) & (! defined(Q_OS_OSX)) & (!HAVE_CANBERRA))
static const char* _key_app_alert_cmd_def = "play /usr/share/sounds/freedesktop/stereo/bell.oga";
#else
static const char* _key_app_alert_cmd_def = "";
#endif

// following are not intended to be edited externally
static const char* _key_app_settings_tree_state = "main_window/settings_tree_state";
static const char* _key_app_geometry = "main_window/non_maximized_geometry";
static const char* _key_app_state = "main_window/state";

static const char* _key_batch_dialog_start_from_current = "batch_dialog/start_from_current_page";
static const bool _key_batch_dialog_start_from_current_def = true;
static const char* _key_batch_dialog_remember_choice = "batch_dialog/remember_choice";
static const bool _key_batch_dialog_remember_choice_def = false;
static const char* _key_batch_processing_priority = "settings/batch_processing_priority";

/* Thumbnails */

static const char* _key_thumbnails_category = "thumbnails/";
static const char* _key_thumbnails_max_cache_pixmap_size = "thumbnails/max_cache_pixmap_size";
static const QSize _key_thumbnails_max_cache_pixmap_size_def = QSize(200, 200);
static const char* _key_thumbnails_max_thumb_size = "thumbnails/max_thumb_size";
static const QSizeF _key_thumbnails_max_thumb_size_def = QSizeF(250., 160.);
static const char* _key_thumbnails_non_focused_selection_highlight_color_adj = "thumbnails/non_focused_selection_highlight_color_adj";
static const int _key_thumbnails_non_focused_selection_highlight_color_adj_def = 40;
static const char* _key_thumbnails_multiple_items_in_row = "thumbnails/list_multiple_items_in_row";
static const bool _key_thumbnails_multiple_items_in_row_def = true;
static const char* _key_thumbnails_min_spacing = "thumbnails/min_spacing";
static const int _key_thumbnails_min_spacing_def = 3;
static const char* _key_thumbnails_boundary_adj_top = "thumbnails/boundary_adj_top";
static const int _key_thumbnails_boundary_adj_top_def = 5;
static const char* _key_thumbnails_boundary_adj_bottom = "thumbnails/boundary_adj_bottom";
static const int _key_thumbnails_boundary_adj_bottom_def = 5;
static const char* _key_thumbnails_boundary_adj_left = "thumbnails/boundary_adj_left";
static const int _key_thumbnails_boundary_adj_left_def  = 5;
static const char* _key_thumbnails_boundary_adj_right = "thumbnails/boundary_adj_right";
static const int _key_thumbnails_boundary_adj_right_def = 3;
static const char* _key_thumbnails_fixed_thumb_size = "thumbnails/fixed_thumb_size";
static const bool _key_thumbnails_fixed_thumb_size_def = false;
static const char* _key_thumbnails_display_order_hints = "thumbnails/display_order_hints";
static const bool _key_thumbnails_display_order_hints_def = true;
static const char* _key_thumbnails_simulate_key_press_hint = "thumbnails/selection_key_press_simulation_hint";
static const bool _key_thumbnails_simulate_key_press_hint_def = true;

/* Export images dialog */

static const char* _key_export_default_output_folder = "settings/default_output_folder";
static const char* _key_export_generate_blank_subscans = "settings/generate_blank_back_subscans";
static const bool  _key_export_generate_blank_subscans_def = false;
static const char* _key_export_use_sep_suffix = "settings/use_sep_suffix_for_background_subscans";
static const bool  _key_export_use_sep_suffix_def = true;
static const char* _key_export_keep_original_color = "settings/keep_original_color_illum_fore_subscans";
static const bool  _key_export_keep_original_color_def = false;
static const char* _key_export_to_multipage = "settings/export_to_multipage";
static const bool  _key_export_to_multipage_def = false;
static const char* _key_export_split_mixed_settings = "settings/split_mixed_settings";
namespace exporting {
static const int _key_export_split_mixed_settings_def = (int) ExportModes(ExportMode::Foreground | ExportMode::Background);
}

/* insert image disalog */

static const char* _key_dont_use_native_dialog = "add_file_dlg/dont_use_native_dialog";
static const bool _key_dont_use_native_dialog_def = true;

/* Page split */

static const char* _key_page_split_apply_cut_enabled = "apply_cut/enabled";
static const bool _key_page_split_apply_cut_enabled_def = true;
static const char* _key_page_split_apply_cut_default = "apply_cut/default";
static const bool _key_page_split_apply_cut_default_def = false;

/* Content zone selection */

static const char* _key_content_sel_STE_tweak = "content_selection/force_STE_reload_tweak";
static const bool _key_content_sel_STE_tweak_def = false;
static const char* _key_content_sel_page_detection_enabled = "page_detection/enabled";
static const bool _key_content_sel_page_detection_enabled_def = false;
static const char* _key_content_sel_page_detection_fine_tune_corners = "page_detection/fine_tune_page_corners";
static const bool _key_content_sel_page_detection_fine_tune_corners_def = false;
static const char* _key_content_sel_page_detection_borders = "page_detection/borders";
static const bool _key_content_sel_page_detection_borders_def = false;
static const char* _key_content_sel_content_color = "content_selection/content_color";
static const char* _key_content_sel_content_color_def = "#320000ff";

static const char* _key_content_sel_page_detection_fine_tune_corners_is_on_by_def = "page_detection/fine_tune_page_corners/default";
static const bool _key_content_sel_page_detection_fine_tune_corners_is_on_by_def_def = false;
static const char* _key_content_sel_page_detection_target_page_size_enabled = "page_detection/target_page_size/enabled";
static const bool _key_content_sel_page_detection_target_page_size_enabled_def = false;
static const char* _key_content_sel_page_detection_target_page_size = "page_detection/target_page_size";
static const QSizeF _key_content_sel_page_detection_target_page_size_def = QSizeF(210., 297.);
static const char* _key_content_sel_page_detection_borders_top = "page_detection/borders/top";
static const double _key_content_sel_page_detection_borders_top_def = 0.;
static const char* _key_content_sel_page_detection_borders_left = "page_detection/borders/left";
static const double _key_content_sel_page_detection_borders_left_def = 0.;
static const char* _key_content_sel_page_detection_borders_right = "page_detection/borders/right";
static const double _key_content_sel_page_detection_borders_right_def = 0.;
static const char* _key_content_sel_page_detection_borders_bottom = "page_detection/borders/bottom";
static const double _key_content_sel_page_detection_borders_bottom_def = 0.;

/* Page Layout: Margins */

static const char* _key_margins_default_top = "margins/default_top";
static const double _key_margins_default_top_def = 0.;
static const char* _key_margins_default_bottom = "margins/default_bottom";
static const double _key_margins_default_bottom_def = 0.;
static const char* _key_margins_default_left = "margins/default_left";
static const double _key_margins_default_left_def = 0.;
static const char* _key_margins_default_right = "margins/default_right";
static const double _key_margins_default_right_def = 0.;
static const char* _key_margins_default_units = "margins/default_units";
static const int _key_margins_default_units_def = 0;
static const char* _key_margins_auto_margins_enabled = "margins/auto_margins_enabled";
static const bool _key_margins_auto_margins_enabled_def = false;
static const char* _key_margins_auto_margins_default = "margins/default_auto_margins";
static const bool _key_margins_auto_margins_default_def = false;
static const char* _key_margins_linked_hor = "margins/leftRightLinked";
static const bool _key_margins_linked_hor_def = true;
static const char* _key_margins_linked_ver = "margins/topBottomLinked";
static const bool _key_margins_linked_ver_def = true;

/* Page Layout: Alignment */

static const char* _key_alignment_original_enabled =  "alignment/original_enabled";
static const bool  _key_alignment_original_enabled_def = false;
static const char* _key_alignment_automagnet_enabled =  "alignment/automagnet_enabled";
static const bool  _key_alignment_automagnet_enabled_def = false;
static const char* _key_alignment_default_alig_vert = "alignment/default_alignment_vert";
static const char* _key_alignment_default_alig_hor = "alignment/default_alignment_hor";
static const char* _key_alignment_default_alig_null = "alignment/default_alignment_null";
static const bool _key_alignment_default_alig_null_def = false;
static const char* _key_alignment_default_alig_tolerance = "alignment/default_alignment_tolerance";
static const double _key_alignment_default_alig_tolerance_def = 0.2;

/* Output stage */

static const char* _key_output_show_orig_on_space = "output/display_orig_page_on_key_press";
static const bool _key_output_show_orig_on_space_def = false;
static const char* _key_output_default_dpi_x = "output/default_dpi_x";
static const int _key_output_default_dpi_x_def = 600;
static const char* _key_output_default_dpi_y = "output/default_dpi_y";
static const int _key_output_default_dpi_y_def = 600;
static const char* _key_output_bin_threshold_min = "output/binrization_threshold_control_min";
static const int _key_output_bin_threshold_min_def = -50;
static const char* _key_output_bin_threshold_max = "output/binrization_threshold_control_max";
static const int _key_output_bin_threshold_max_def = 50;
static const char* _key_output_bin_threshold_default = "output/binrization_threshold_control_default";
static const int _key_output_bin_threshold_default_def = 0;
static const char* _key_output_despeckling_default_lvl = "despeckling/default_level";
static const output::DespeckleLevel _key_output_despeckling_default_lvl_def = output::DespeckleLevel::DESPECKLE_CAUTIOUS;
static const char* _key_output_foreground_layer_control_threshold = "foreground_layer/control_threshold";
static const bool _key_output_foreground_layer_control_threshold_def = false;

static const char* _key_output_foreground_layer_enabled = "foreground_layer/enabled";
static const bool _key_output_foreground_layer_enabled_def = true;
static const char* _key_output_picture_layer_enabled = "picture_zones_layer/enabled";
static const bool _key_output_picture_layer_enabled_def = true;

/* Dewarping */

static const char* _key_dewarp_auto_vert_half_correction = "dewarp/auto_vert_half_correction";
static const bool _key_dewarp_auto_vert_half_correction_def = false;
static const char* _key_dewarp_auto_deskew_after_dewarp = "dewarp/auto_deskew_after_dewarp";
static const bool _key_dewarp_auto_deskew_after_dewarp_def = false;
/* Misc */

static const char* _key_autosave_inputdir = "auto-save_project/_inputDir";
static const char* _key_autosave_enabled = "auto-save_project/enabled";
static const bool _key_autosave_enabled_def = false;
static const char* _key_autosave_time_period_min = "auto-save_project/time_period_min";
static const int _key_autosave_time_period_min_def = 5;
static const char* _key_debug_enabled = "debug_mode/enabled";
static const bool _key_debug_enabled_def = false;
static const char* _key_dpi_predefined_list = "dpi/predefined_list";
static const char* _key_dpi_predefined_list_def = "300x300,400x400,600x600";
static const char* _key_dpi_change_list = "dpi/change_dpi_list";
static const char* _key_dpi_change_list_def = "300,400,600" ;
static const char* _key_project_last_dir = "project/lastDir";
static const char* _key_hot_keys_jump_forward_pg_num = "hot_keys/jump_forward_pg_num";
static const int _key_hot_keys_jump_forward_pg_num_def = 5;
static const char* _key_hot_keys_jump_backward_pg_num = "hot_keys/jump_backward_pg_num";
static const char* _key_hot_keys_scheme_ver = "hot_keys/scheme_ver";
static const char* _key_hot_keys_cnt = "hot_keys/count";
static const char* _key_hot_keys_group_id = "hot_keys/group_%1";
static const int _key_hot_keys_jump_backward_pg_num_def = 5;
static const char* _key_project_last_input_dir = "lastInputDir";
static const char* _key_recent_projects = "project/recent";
static const char* _key_recent_projects_item = "path";
static const char* _key_use_3d_accel = "settings/use_3d_acceleration";
static const bool _key_use_3d_accel_def = false;

static const char* _key_deskew_controls_color = "deskew/controls_color";
static const char* _key_deskew_controls_color_def = "#5A0000ff";
static const char* _key_deskew_deviant_enabled = "deskew_deviant/enabled";
static const bool _key_deskew_deviant_enabled_def = false;
static const char* _key_select_content_deviant_enabled = "select_content_deviant/enabled";
static const bool _key_select_content_deviant_enabled_def = false;
static const char* _key_margins_deviant_enabled = "margins_deviant/enabled";
static const bool _key_margins_deviant_enabled_def = false;

static const char* _key_tiff_compr_method_bw = "tiff_compression/method";
static const char* _key_tiff_compr_method_bw_def = "LZW";
static const char* _key_tiff_compr_method_color = "tiff_compression/method_color";
static const char* _key_tiff_compr_method_color_def = "LZW";
static const char* _key_tiff_compr_horiz_pred = "tiff_compression/use_horizontal_predictor";
static const bool _key_tiff_compr_horiz_pred_def = false;
static const char* _key_tiff_compr_show_all = "tiff_compression/show_all";
static const bool _key_tiff_compr_show_all_def = false;

static const char* _key_mode_bw_disable_smoothing = "mode_bw/disable_smoothing";
static const bool _key_mode_bw_disable_smoothing_def = false;
static const char* _key_zone_editor_min_angle = "zone_editor/min_angle";
static const float _key_zone_editor_min_angle_def = 3.;
static const char* _key_picture_zones_layer_sensitivity = "picture_zones_layer/sensitivity";
static const int _key_picture_zones_layer_sensitivity_def = 100;
static const char* _key_foreground_layer_adj_override = "foreground_layer_adj_override";
static const int _key_foreground_layer_adj_override_def = 0;
static const char* _key_mouse_ignore_system_wheel_settings = "mouse/ignore_system_wheel_settings";
static const bool _key_mouse_ignore_system_wheel_settings_def = true;

// DjVu

static const char* _key_djvu_bin_minidjvu = "djvu_minidjvu";
static const char* _key_djvu_bin_c44 = "djvu_c44";
static const char* _key_djvu_bin_djvuextract= "djvu_djvuextract";
static const char* _key_djvu_bin_djvumake = "djvu_djvumake";
static const char* _key_djvu_bin_djvused = "djvu_djvused";
static const char* _key_djvu_bin_tesseract = "djvu_tesseract";
static const char* _key_djvu_bin_djvm = "djvu_djvm";
#ifdef _WIN32
static const char* _key_djvu_bin_minidjvu_def = "./bin/minidjvu-mod";
static const char* _key_djvu_bin_c44_def = "./bin/c44-fi";
static const char* _key_djvu_bin_djvuextract_def = "./bin/djvuextract";
static const char* _key_djvu_bin_djvumake_def = "./bin/djvumake";
static const char* _key_djvu_bin_djvused_def = "./bin/djvused";
static const char* _key_djvu_bin_tesseract_def = "./bin/tesseract";
static const char* _key_djvu_bin_djvm_def = "./bin/djvm";
#else
static const char* _key_djvu_bin_minidjvu_def = "minidjvu-mod";
static const char* _key_djvu_bin_c44_def = "c44-fi";
static const char* _key_djvu_bin_djvuextract_def = "djvuextract";
static const char* _key_djvu_bin_djvumake_def = "djvumake";
static const char* _key_djvu_bin_djvused_def = "djvused";
static const char* _key_djvu_bin_tesseract_def = "tesseract";
static const char* _key_djvu_bin_djvm_def = "djvm";
#endif
static const char* _key_djvu_pages_subfolder = "djvu_pages_subfolder";
static const char* _key_djvu_pages_subfolder_def = "djvu";
static const char* _key_djvu_layers_subfolder = "djvu_layers_subfolder";
static const char* _key_djvu_layers_subfolder_def = "layers";
static const char* _key_djvu_pages_per_djbz = "djvu_pages_per_djbz";
static const int _key_djvu_pages_per_djbz_def = 20;
static const char* _key_djvu_djbz_erosion = "djvu_djbz_erosion";
static const bool _key_djvu_djbz_erosion_def = false;
static const char* _key_djvu_djbz_use_prototypes = "djvu_djbz_use_prototypes";
static const bool _key_djvu_djbz_use_prototypes_def = true;
static const char* _key_djvu_djbz_use_averaging = "djvu_djbz_use_averaging";
static const bool _key_djvu_djbz_use_averaging_def = true;
static const char* _key_djvu_djbz_aggression = "djvu_djbz_aggression";
static const int _key_djvu_djbz_aggression_def = 100;
static const char* _key_djvu_djbz_classifier = "djvu_djbz_classifier";
static const int _key_djvu_djbz_classifier_def = publish::DjbzParams::ClassifierType::Maximal;
static const char* _key_djvu_djbz_extension = "djvu_djbz_extension";
static const char* _key_djvu_djbz_extension_def = "djbz";

static const char* _key_scale_bsf = "bsf";
static const int _key_scale_bsf_def = 1;
static const char* _key_scale_filter = "scale_filter";
static const FREE_IMAGE_FILTER _key_scale_filter_def = FREE_IMAGE_FILTER::FILTER_BICUBIC;


#endif // INI_KEYS_H
