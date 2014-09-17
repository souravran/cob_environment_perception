/*!
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2012 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 *  Project name: care-o-bot
 * \note
 *  ROS stack name: cob_environment_perception
 * \note
 *  ROS package name: cob_3d_mapping_semantics
 *
 * \author
 *  Author: Waqas Tanveer email:Waqas.Tanveer@ipa.fraunhofer.de
 * \author
 *  Supervised by: Georg Arbeiter, email:georg.arbeiter@ipa.fhg.de
 *
 * \date Date of creation: 11/2011
 *
 * \brief
 * Description:
 *
 * ToDo:
 *
 *
 *
 *****************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer. \n
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution. \n
 *     - Neither the name of the Fraunhofer Institute for Manufacturing
 *       Engineering and Automation (IPA) nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission. \n
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/

//##################
//#### includes ####
#include <algorithm>

// ROS includes
#include <ros/ros.h>
#include <rosbag/bag.h>
#include <dynamic_reconfigure/server.h>

// ros message includes
#include <sensor_msgs/PointCloud.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

// PCL includes
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/ros/conversions.h>
#include <pcl/io/pcd_io.h>

//internal includes
#include <cob_3d_mapping_msgs/ShapeArray.h>
#include <cob_3d_mapping_msgs/GetGeometryMap.h>
#include <cob_3d_mapping_msgs/GetTables.h>
#include <cob_3d_mapping_semantics/table_extraction.h>
#include <cob_3d_mapping_semantics/table_extraction_nodeConfig.h>
#include <cob_3d_mapping_common/ros_msg_conversions.h>
#include <cob_3d_visualization/simple_marker.h>

using namespace cob_3d_mapping;
using namespace cob_3d_visualization;

class EnvironmentMonitoringNode
{
	struct Area {
		std_msgs::ColorRGBA color;
		float r1, r2;
	};
public:

  // Constructor
  EnvironmentMonitoringNode () :
    target_frame_id_ ("/map")
  {
    config_server_.setCallback(boost::bind(&EnvironmentMonitoringNode::dynReconfCallback, this, _1, _2));

    sa_sub_ = n_.subscribe ("shape_array", 10, &EnvironmentMonitoringNode::callbackShapeArray, this);
    sa_pub_ = n_.advertise<cob_3d_mapping_msgs::ShapeArray> ("shape_tables_array_pub", 1);
    sa_combined_pub_ = n_.advertise<cob_3d_mapping_msgs::ShapeArray> ("shape_combined_array_pub", 1);
    
    marker_vis_sub_ = n_.subscribe("vis_markers_sub", 10, &EnvironmentMonitoringNode::callbackVisMarkerArray, this);
    marker_vis_pub_ = n_.advertise<visualization_msgs::MarkerArray>("vis_markers_pub",10);

    get_tables_server_ = n_.advertiseService ("get_tables", &EnvironmentMonitoringNode::getTablesService, this);
    
    color_table_.push_back(0);//r
    color_table_.push_back(0);//g
    color_table_.push_back(1);//b
    color_table_.push_back(1);//a
    
    Area a;
    a.r1 = 0;
    a.r2 = 1.5;
    a.color.r = 1;
    a.color.a = 1;
    areas_.push_back(a);
    
    a.r1 = 1.5;
    a.r2 = 2.5;
    a.color.g = 1;
    areas_.push_back(a);
    
    publishAreas();
  }

  // Destructor
  ~EnvironmentMonitoringNode ()
  {
    /// void
  }
  
  void callbackVisMarkerArray(visualization_msgs::MarkerArray ma) {
	  const float blend = 0.5f;
	  
	  for(size_t i=0; i<ma.markers.size(); i++) {
		  if(ma.markers[i].type != visualization_msgs::Marker::TRIANGLE_LIST) continue;
		  if(ma.markers[i].pose.position.z < te_.getHeightMin()) continue; //floor
		  
		  ma.markers[i].colors.resize(ma.markers[i].points.size(), ma.markers[i].color);
		  
		  Eigen::Affine3d T;
		  tf::poseMsgToEigen(ma.markers[i].pose, T);
		  
		  for(size_t j=0; j<ma.markers[i].points.size(); j++) {
			  Eigen::Vector3d v;
			  v(0) = ma.markers[i].points[j].x;
			  v(1) = ma.markers[i].points[j].y;
			  v(2) = ma.markers[i].points[j].z;
			  Eigen::Affine3d T;
			  tf::poseMsgToEigen(ma.markers[i].pose, T);
			  v = (T*v).eval();
			  v(2)=0;
			  const double d = v.norm();
			  
			  for(size_t k=0; k<areas_.size(); k++)
				  if(areas_[k].r1<=d && areas_[k].r2>d) {
					  ma.markers[i].colors[j].r = areas_[k].color.r*blend + ma.markers[i].colors[j].r*(1-blend);
					  ma.markers[i].colors[j].g = areas_[k].color.g*blend + ma.markers[i].colors[j].g*(1-blend);
					  ma.markers[i].colors[j].b = areas_[k].color.b*blend + ma.markers[i].colors[j].b*(1-blend);
					  ma.markers[i].colors[j].a = areas_[k].color.a*blend + ma.markers[i].colors[j].a*(1-blend);
					  break;
				  }
				  
		  }
	  }
	  marker_vis_pub_.publish(ma);
  }
  
  void publishAreas() {
	  RvizMarkerManager::get().clear();
	  for(size_t a=0; a<areas_.size(); a++) {
		RvizMarker m;
		m.circle(areas_[a].r2, Eigen::Vector3f::UnitZ()*0.05f, areas_[a].r1);
		m.color(areas_[a].color);
	  }
	  RvizMarkerManager::get().publish();
  }

  /**
   * @brief callback for dynamic reconfigure
   *
   * everytime the dynamic reconfiguration changes this function will be called
   *
   * @param config data of configuration
   * @param level bit descriptor which notifies which parameter changed
   *
   * @return nothing
   */
  void
  dynReconfCallback(cob_3d_mapping_semantics::table_extraction_nodeConfig &config, uint32_t level)
  {
    ROS_INFO("[table_extraction]: received new parameters");
    target_frame_id_ = config.target_frame_id;
    te_.setNormalBounds (config.tilt_angle);
    te_.setHeightMin (config.height_min);
    te_.setHeightMax (config.height_max);
    te_.setAreaMin (config.area_min);
    te_.setAreaMax (config.area_max);
    blend_ = config.blend;
    if(areas_.size()>1) {
		areas_[1].r1 = areas_[0].r2 = config.radius_inner;
		areas_[1].r2 = config.radius_outer;
    }
    
    publishAreas();
  }

  /**
   * @brief callback for ShapeArray messages
   *
   * @param sa_ptr pointer to the message
   *
   * @return nothing
   */

  void
  callbackShapeArray (cob_3d_mapping_msgs::ShapeArray sa_ptr)
  {
    if(sa_ptr.header.frame_id != target_frame_id_)
    {
      ROS_ERROR("Frame IDs do not match, aborting...");
      return;
    }
    cob_3d_mapping_msgs::ShapeArray tables;
    tables.header = sa_ptr.header;
    tables.header.frame_id = target_frame_id_ ;
    processMap(sa_ptr, tables);
    sa_pub_.publish(tables);
    sa_combined_pub_.publish(sa_ptr);
    ROS_INFO("Found %u tables", (unsigned int)tables.shapes.size());
  }


  /**
   * @brief service offering table object candidates
   *
   * @param req request for objects of a class (table objects in this case)
   * @param res response of the service which is possible table object candidates
   *
   * @return true if service successful
   */
  bool
  getTablesService (cob_3d_mapping_msgs::GetTablesRequest &req,
                    cob_3d_mapping_msgs::GetTablesResponse &res)
  {
    ROS_INFO("table detection started...");

    cob_3d_mapping_msgs::ShapeArray sa;
    if (getMapService (sa))
    {
    	if(sa.header.frame_id != target_frame_id_)
    	{
      		ROS_ERROR("Frame IDs do not match, aborting...");
      		return false;
    	}
      processMap(sa, res.tables);
      res.tables.header = sa.header;
      ROS_INFO("Found %zu tables", res.tables.shapes.size());
      return true;
    }
    else
      return false;
  }

  /**
   * @brief service offering geometry map of the scene
   *
   * @return true if service successful
   */
  bool
  getMapService (cob_3d_mapping_msgs::ShapeArray& sa)
  {
    ROS_INFO("Waiting for service server to start.");
    ros::service::waitForService ("get_geometry_map", 10); //will wait for infinite time

    ROS_INFO("Server started, polling map.");

    //build message
    cob_3d_mapping_msgs::GetGeometryMapRequest req;
    cob_3d_mapping_msgs::GetGeometryMapResponse res;

    if (ros::service::call ("get_geometry_map", req, res))
    {
      ROS_INFO("Service call finished.");
    }
    else
    {
      ROS_INFO("Service call failed.");
      return 0;
    }
    sa = res.map;
    return true;
  }

  /**
   * @brief processes a shape array in order to find tables
   *
   * @param sa input shape array
   * @param tables output shape array containing tables
   *
   * @return nothing
   */
  void
  processMap(cob_3d_mapping_msgs::ShapeArray& sa, cob_3d_mapping_msgs::ShapeArray& tables)
  {
    for (unsigned int i = 0; i < sa.shapes.size (); i++)
    {
      Polygon::Ptr poly_ptr (new Polygon());
      fromROSMsg(sa.shapes[i], *poly_ptr);
      te_.setInputPolygon(poly_ptr);
      if (te_.isTable())
      {
        poly_ptr->color_ = color_table_;
        cob_3d_mapping_msgs::Shape s;
        s.header = sa.header;
        s.header.frame_id = target_frame_id_;
        toROSMsg(*poly_ptr,s);
        ROS_INFO("getTablesService: Polygon[%d] converted to shape",i);
        tables.shapes.push_back (s);
        sa.shapes[i] = s;
      }
    }
  }

  ros::NodeHandle n_;

protected:
  ros::Subscriber sa_sub_, marker_vis_sub_;
  ros::Publisher sa_pub_, sa_combined_pub_, marker_vis_pub_;
  ros::ServiceServer get_tables_server_;

  /**
  * @brief Dynamic Reconfigure server
  */
  dynamic_reconfigure::Server<cob_3d_mapping_semantics::table_extraction_nodeConfig> config_server_;

  TableExtraction te_;

  std::string target_frame_id_;
  std::vector<Area> areas_;
  std::vector<float> color_table_;
  float blend_;
};

int
main (int argc, char** argv)
{
  ros::init (argc, argv, "table_extraction_node");
  RvizMarkerManager::get().createTopic("/extra_objects_marker").setFrameId("/map").clearOld();

  EnvironmentMonitoringNode sem_exn_node;
  ros::spin ();
}
