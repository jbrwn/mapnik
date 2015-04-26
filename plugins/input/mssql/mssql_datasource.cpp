// file plugin
#include "mssql_datasource.hpp"
#include "mssql_featureset.hpp"
#include "mssql_connection.hpp"

// boost


using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(mssql_datasource)

mssql_datasource::mssql_datasource(parameters const& params)
    : datasource(params),
      table_(*params.get<std::string>("table","")),
      geometry_field_(*params.get<std::string>("geometry_field","")),
      sort_field_(*params.get<std::string>("sort_field","")),
      sort_order_(*params.get<std::string>("sort_order","ASC")),
      extent_(),
      desc_(mssql_datasource::name(),*params.get<std::string>("encoding","utf-8"))
{


    //connection_string must not be empty
    //table must not be empty
    
    //geometry_field must not be empty
    //extent must not be empty
    
    //create connection
    
    // TO DO: automatically detect geometry field if empty
    // TO DO: automatically detect extent if empty
    
    //get column descriptions
}

mssql_datasource::~mssql_datasource() { }

const char * mssql_datasource::name()
{
    return "mssql";
}

mapnik::datasource::datasource_t mssql_datasource::type() const
{
    return datasource::Vector;
}

mapnik::box2d<double> mssql_datasource::envelope() const
{
    return extent_;
}

boost::optional<mapnik::datasource_geometry_t> mssql_datasource::get_geometry_type() const
{
    boost::optional<mapnik::datasource_geometry_t> result;
    return result;
}

mapnik::layer_descriptor mssql_datasource::get_descriptor() const
{
    return desc_;
}

mapnik::featureset_ptr mssql_datasource::features(mapnik::query const& q) const
{
    // if the query box intersects our world extent then query for features
    if (extent_.intersects(q.get_bbox()))
    {
        return std::make_shared<mssql_featureset>(q.get_bbox(),desc_.get_encoding());
    }
    
    // otherwise return an empty featureset pointer
    return mapnik::featureset_ptr();
}

mapnik::featureset_ptr mssql_datasource::features_at_point(mapnik::coord2d const& pt, double tol) const
{
    // features_at_point is rarely used - only by custom applications,
    // so for this sample plugin let's do nothing...
    return mapnik::featureset_ptr();
}