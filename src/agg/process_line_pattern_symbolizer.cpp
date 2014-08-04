/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2011 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

// mapnik
#include <mapnik/feature.hpp>
#include <mapnik/debug.hpp>
#include <mapnik/graphics.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/agg_rasterizer.hpp>
#include <mapnik/agg_pattern_source.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/vertex_converters.hpp>
#include <mapnik/noncopyable.hpp>
#include <mapnik/parse_path.hpp>

// agg
#include "agg_basics.h"
#include "agg_pixfmt_rgba.h"
#include "agg_color_rgba.h"
#include "agg_rendering_buffer.h"
#include "agg_rasterizer_outline.h"
#include "agg_rasterizer_outline_aa.h"
#include "agg_scanline_u.h"
#include "agg_renderer_scanline.h"
#include "agg_pattern_filters_rgba.h"
#include "agg_span_allocator.h"
#include "agg_span_pattern_rgba.h"
#include "agg_renderer_outline_image.h"
#include "agg_conv_clip_polyline.h"

// boost


namespace {

class pattern_source : private mapnik::noncopyable
{
public:
    pattern_source(mapnik::image_data_32 const& pattern)
        : pattern_(pattern) {}

    inline unsigned int width() const
    {
        return pattern_.width();
    }
    inline unsigned int height() const
    {
        return pattern_.height();
    }
    inline agg::rgba8 pixel(int x, int y) const
    {
        unsigned c = pattern_(x,y);
        return agg::rgba8_pre(c & 0xff,
                          (c >> 8) & 0xff,
                          (c >> 16) & 0xff,
                          (c >> 24) & 0xff);
    }
private:
    mapnik::image_data_32 const& pattern_;
};

}

namespace mapnik {

template <typename T0, typename T1>
void  agg_renderer<T0,T1>::process(line_pattern_symbolizer const& sym,
                               mapnik::feature_impl & feature,
                               proj_transform const& prj_trans)
{

    using color = agg::rgba8;
    using order = agg::order_rgba;
    using blender_type = agg::comp_op_adaptor_rgba_pre<color, order>;
    using pattern_filter_type = agg::pattern_filter_bilinear_rgba8;
    using pattern_type = agg::line_image_pattern<pattern_filter_type>;
    using pixfmt_type = agg::pixfmt_custom_blend_rgba<blender_type, agg::rendering_buffer>;
    using renderer_base = agg::renderer_base<pixfmt_type>;
    using renderer_type = agg::renderer_outline_image<renderer_base, pattern_type>;
    using rasterizer_type = agg::rasterizer_outline_aa<renderer_type>;

    std::string filename = get<std::string>(sym, keys::file, feature, common_.vars_);
    if (filename.empty()) return;
    boost::optional<mapnik::marker_ptr> marker_ptr = marker_cache::instance().find(filename, true);
    if (!marker_ptr || !(*marker_ptr)) return;
    boost::optional<image_ptr> pat;
    if ((*marker_ptr)->is_bitmap())
    {
        pat = (*marker_ptr)->get_bitmap_data();
    }
    else
    {
        agg::trans_affine image_tr = agg::trans_affine_scaling(common_.scale_factor_);
        auto image_transform = get_optional<transform_type>(sym, keys::image_transform);
        if (image_transform) evaluate_transform(image_tr, feature, common_.vars_, *image_transform);
        pat = render_pattern(*ras_ptr, **marker_ptr, image_tr);
    }

    if (!pat) return;

    bool clip = get<value_bool>(sym, keys::clip, feature, common_.vars_, false);
    //double opacity = get<value_double>(sym,keys::stroke_opacity,feature, 1.0); TODO
    double offset = get<value_double>(sym, keys::offset, feature, common_.vars_, 0.0);
    double simplify_tolerance = get<value_double>(sym, keys::simplify_tolerance, feature, common_.vars_, 0.0);
    double smooth = get<value_double>(sym, keys::smooth, feature, common_.vars_, false);

    agg::rendering_buffer buf(current_buffer_->raw_data(),current_buffer_->width(),current_buffer_->height(), current_buffer_->width() * 4);
    pixfmt_type pixf(buf);
    pixf.comp_op(static_cast<agg::comp_op_e>(get<composite_mode_e>(sym, keys::comp_op, feature, common_.vars_, src_over)));
    renderer_base ren_base(pixf);
    agg::pattern_filter_bilinear_rgba8 filter;

    pattern_source source(*(*pat));
    pattern_type pattern (filter,source);
    renderer_type ren(ren_base, pattern);
    rasterizer_type ras(ren);

    agg::trans_affine tr;
    auto transform = get_optional<transform_type>(sym, keys::geometry_transform);
    if (transform) evaluate_transform(tr, feature, common_.vars_, *transform, common_.scale_factor_);

    box2d<double> clip_box = clipping_extent();
    if (clip)
    {
        double padding = (double)(common_.query_extent_.width()/pixmap_.width());
        double half_stroke = (*marker_ptr)->width()/2.0;
        if (half_stroke > 1)
            padding *= half_stroke;
        if (std::fabs(offset) > 0)
            padding *= std::fabs(offset) * 1.2;
        padding *= common_.scale_factor_;
        clip_box.pad(padding);
    }

    using conv_types = boost::mpl::vector<clip_line_tag,transform_tag,offset_transform_tag,affine_transform_tag,simplify_tag,smooth_tag>;
    vertex_converter<box2d<double>, rasterizer_type, line_pattern_symbolizer,
                     CoordTransform, proj_transform, agg::trans_affine, conv_types, feature_impl>
        converter(clip_box,ras,sym,common_.t_,prj_trans,tr,feature,common_.vars_,common_.scale_factor_);

    if (clip) converter.set<clip_line_tag>(); //optional clip (default: true)
    converter.set<transform_tag>(); //always transform
    if (simplify_tolerance > 0.0) converter.set<simplify_tag>(); // optional simplify converter
    if (std::fabs(offset) > 0.0) converter.set<offset_transform_tag>(); // parallel offset
    converter.set<affine_transform_tag>(); // optional affine transform
    if (smooth > 0.0) converter.set<smooth_tag>(); // optional smooth converter

    for (geometry_type & geom : feature.paths())
    {
        if (geom.size() > 1)
        {
            converter.apply(geom);
        }
    }
}

template void agg_renderer<image_32>::process(line_pattern_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);

}
