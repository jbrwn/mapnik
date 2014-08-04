/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
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
#include <mapnik/text/symbolizer_helpers.hpp>
#include <mapnik/label_collision_detector.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/text/text_layout.hpp>
#include <mapnik/geom_util.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/debug.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/value_types.hpp>
#include <mapnik/text/placement_finder.hpp>
#include <mapnik/text/placements/base.hpp>
#include <mapnik/text/placements/dummy.hpp>

//agg
#include "agg_conv_clip_polyline.h"

namespace mapnik {

base_symbolizer_helper::base_symbolizer_helper(
        symbolizer_base const& sym,
        feature_impl const& feature,
        attributes const& vars,
        proj_transform const& prj_trans,
        unsigned width, unsigned height, double scale_factor,
        CoordTransform const& t,
        box2d<double> const& query_extent)
    : sym_(sym),
      feature_(feature),
      vars_(vars),
      prj_trans_(prj_trans),
      t_(t),
      dims_(0, 0, width, height),
      query_extent_(query_extent),
      scale_factor_(scale_factor),
      clipped_(get<bool>(sym_, keys::clip, feature_, vars_, false)),
      placement_(get<text_placements_ptr>(sym_, keys::text_placements_)->get_placement_info(scale_factor))
{
    placement_->properties.evaluate_text_properties(feature_, vars_);
    initialize_geometries();
    if (!geometries_to_process_.size()) return; // FIXME - bad practise
    initialize_points();
}

struct largest_bbox_first
{
    bool operator() (geometry_type const* g0, geometry_type const* g1) const
    {
        box2d<double> b0 = g0->envelope();
        box2d<double> b1 = g1->envelope();
        return b0.width()*b0.height() > b1.width()*b1.height();
    }

};

void base_symbolizer_helper::initialize_geometries()
{
    bool largest_box_only = placement_->properties.largest_bbox_only;
    double minimum_path_length = placement_->properties.minimum_path_length;
    for ( auto const& geom :  feature_.paths())
    {
        // don't bother with empty geometries
        if (geom.size() == 0) continue;
        mapnik::geometry_type::types type = geom.type();
        if (type == geometry_type::types::Polygon)
        {
            if (minimum_path_length > 0)
            {
                box2d<double> gbox = t_.forward(geom.envelope(), prj_trans_);
                if (gbox.width() < minimum_path_length)
                {
                    continue;
                }
            }
        }
        // TODO - calculate length here as well
        geometries_to_process_.push_back(const_cast<geometry_type*>(&geom));
    }

    if (largest_box_only)
    {
        geometries_to_process_.sort(largest_bbox_first());
        geo_itr_ = geometries_to_process_.begin();
        geometries_to_process_.erase(++geo_itr_,geometries_to_process_.end());
    }
    geo_itr_ = geometries_to_process_.begin();
}

void base_symbolizer_helper::initialize_points()
{
    label_placement_enum how_placed = placement_->properties.label_placement;
    if (how_placed == LINE_PLACEMENT)
    {
        point_placement_ = false;
        return;
    }
    else
    {
        point_placement_ = true;
    }

    double label_x=0.0;
    double label_y=0.0;
    double z=0.0;

    for (auto * geom_ptr : geometries_to_process_)
    {
        geometry_type const& geom = *geom_ptr;
        if (how_placed == VERTEX_PLACEMENT)
        {
            geom.rewind(0);
            for(unsigned i = 0; i < geom.size(); ++i)
            {
                geom.vertex(&label_x, &label_y);
                prj_trans_.backward(label_x, label_y, z);
                t_.forward(&label_x, &label_y);
                points_.push_back(pixel_position(label_x, label_y));
            }
        }
        else
        {
            // https://github.com/mapnik/mapnik/issues/1423
            bool success = false;
            // https://github.com/mapnik/mapnik/issues/1350
            if (geom.type() == geometry_type::types::LineString)
            {
                success = label::middle_point(geom, label_x,label_y);
            }
            else if (how_placed == POINT_PLACEMENT)
            {
                success = label::centroid(geom, label_x, label_y);
            }
            else if (how_placed == INTERIOR_PLACEMENT)
            {
                success = label::interior_position(geom, label_x, label_y);
            }
            else
            {
                MAPNIK_LOG_ERROR(symbolizer_helpers) << "ERROR: Unknown placement type in initialize_points()";
            }
            if (success)
            {
                prj_trans_.backward(label_x, label_y, z);
                t_.forward(&label_x, &label_y);
                points_.push_back(pixel_position(label_x, label_y));
            }
        }
    }
    point_itr_ = points_.begin();
}

template <typename FaceManagerT, typename DetectorT>
text_symbolizer_helper::text_symbolizer_helper(
        text_symbolizer const& sym,
        feature_impl const& feature,
        attributes const& vars,
        proj_transform const& prj_trans,
        unsigned width, unsigned height, double scale_factor,
        CoordTransform const& t, FaceManagerT & font_manager,
        DetectorT &detector, box2d<double> const& query_extent)
    : base_symbolizer_helper(sym, feature, vars, prj_trans, width, height, scale_factor, t, query_extent),
      finder_(feature, vars, detector, dims_, *placement_, font_manager, scale_factor),
      points_on_line_(false)
{
    if (geometries_to_process_.size()) finder_.next_position();
}

placements_list const& text_symbolizer_helper::get()
{
    if (point_placement_)
    {
        while (next_point_placement());
    }
    else
    {
        while (next_line_placement(clipped_));
    }
    return finder_.placements();
}

bool text_symbolizer_helper::next_line_placement(bool clipped)
{
    while (!geometries_to_process_.empty())
    {
        if (geo_itr_ == geometries_to_process_.end())
        {
            //Just processed the last geometry. Try next placement.
            if (!finder_.next_position()) return false; //No more placements
            //Start again from begin of list
            geo_itr_ = geometries_to_process_.begin();
            continue; //Reexecute size check
        }
        bool success = false;
        if (clipped)
        {
            using clipped_geometry_type = agg::conv_clip_polyline<geometry_type>;
            using path_type = coord_transform<CoordTransform,clipped_geometry_type>;

            clipped_geometry_type clipped(**geo_itr_);
            clipped.clip_box(query_extent_.minx(), query_extent_.miny(),
                             query_extent_.maxx(), query_extent_.maxy());
            path_type path(t_, clipped, prj_trans_);
            success = finder_.find_line_placements(path, points_on_line_);
        }
        else
        {
            using path_type = coord_transform<CoordTransform,geometry_type>;
            path_type path(t_, **geo_itr_, prj_trans_);
            success = finder_.find_line_placements(path, points_on_line_);
        }
        if (success)
        {
            //Found a placement
            geo_itr_ = geometries_to_process_.erase(geo_itr_);
            return true;
        }
        //No placement for this geometry. Keep it in geometries_to_process_ for next try.
        ++geo_itr_;
    }
    return false;
}

bool text_symbolizer_helper::next_point_placement()
{
    while (!points_.empty())
    {
        if (point_itr_ == points_.end())
        {
            //Just processed the last point. Try next placement.
            if (!finder_.next_position()) return false; //No more placements
            //Start again from begin of list
            point_itr_ = points_.begin();
            continue; //Reexecute size check
        }
        if (finder_.find_point_placement(*point_itr_))
        {
            //Found a placement
            point_itr_ = points_.erase(point_itr_);
            return true;
        }
        //No placement for this point. Keep it in points_ for next try.
        point_itr_++;
    }
    return false;
}

/*****************************************************************************/

template <typename FaceManagerT, typename DetectorT>
text_symbolizer_helper::text_symbolizer_helper(
        shield_symbolizer const& sym,
        feature_impl const& feature,
        attributes const& vars,
        proj_transform const& prj_trans,
        unsigned width, unsigned height, double scale_factor,
        CoordTransform const& t, FaceManagerT & font_manager,
        DetectorT &detector, const box2d<double> &query_extent)
    : base_symbolizer_helper(sym, feature, vars, prj_trans, width, height, scale_factor, t, query_extent),
      finder_(feature, vars, detector, dims_, *placement_, font_manager, scale_factor),
      points_on_line_(true)
{
    if (geometries_to_process_.size())
    {
        init_marker();
        finder_.next_position();
    }
}


void text_symbolizer_helper::init_marker()
{
    std::string filename = mapnik::get<std::string>(sym_, keys::file, feature_, vars_);
    if (filename.empty()) return;
    boost::optional<mapnik::marker_ptr> marker = marker_cache::instance().find(filename, true);
    if (!marker) return;
    //FIXME - need to test this
    //std::string filename = path_processor_type::evaluate(filename_string, feature_);
    agg::trans_affine trans;
    auto image_transform = get_optional<transform_type>(sym_, keys::image_transform);
    if (image_transform) evaluate_transform(trans, feature_, vars_, *image_transform);
    double width = (*marker)->width();
    double height = (*marker)->height();
    double px0 = - 0.5 * width;
    double py0 = - 0.5 * height;
    double px1 = 0.5 * width;
    double py1 = 0.5 * height;
    double px2 = px1;
    double py2 = py0;
    double px3 = px0;
    double py3 = py1;
    trans.transform(&px0, &py0);
    trans.transform(&px1, &py1);
    trans.transform(&px2, &py2);
    trans.transform(&px3, &py3);
    box2d<double> bbox(px0, py0, px1, py1);
    bbox.expand_to_include(px2, py2);
    bbox.expand_to_include(px3, py3);
    bool unlock_image = mapnik::get<value_bool>(sym_, keys::unlock_image, feature_, vars_, false);
    double shield_dx = mapnik::get<value_double>(sym_, keys::shield_dx, feature_, vars_, 0.0);
    double shield_dy = mapnik::get<value_double>(sym_, keys::shield_dy, feature_, vars_, 0.0);
    pixel_position marker_displacement;
    marker_displacement.set(shield_dx,shield_dy);
    finder_.set_marker(std::make_shared<marker_info>(*marker, trans), bbox, unlock_image, marker_displacement);
}

/*****************************************************************************/

template text_symbolizer_helper::text_symbolizer_helper(
    text_symbolizer const& sym,
    feature_impl const& feature,
    attributes const& vars,
    proj_transform const& prj_trans,
    unsigned width,
    unsigned height,
    double scale_factor,
    CoordTransform const& t,
    face_manager<freetype_engine> &font_manager,
    label_collision_detector4 &detector,
    box2d<double> const& query_extent);

template text_symbolizer_helper::text_symbolizer_helper(
    shield_symbolizer const& sym,
    feature_impl const& feature,
    attributes const& vars,
    proj_transform const& prj_trans,
    unsigned width,
    unsigned height,
    double scale_factor,
    CoordTransform const& t,
    face_manager<freetype_engine> &font_manager,
    label_collision_detector4 &detector,
    box2d<double> const& query_extent);
} //namespace
