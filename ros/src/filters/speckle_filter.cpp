/*
 * speckle_filter.cpp
 *
 *  Created on: Mar 3, 2011
 *      Author: goa-wq
 */

//##################
//#### includes ####

// standard includes
//--

// ROS includes
#include <ros/ros.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

// ROS message includes
#include <sensor_msgs/PointCloud2.h>

// external includes
#include <boost/timer.hpp>
#include <cob_vision_utils/VisionUtils.h>
#include "pcl/filters/statistical_outlier_removal.h"
#include "pcl/point_types.h"
#include <pluginlib/class_list_macros.h>
#include <pcl_ros/pcl_nodelet.h>
#include <cob_env_model/cpc_point.h>

//####################
//#### node class ####
class SpeckleFilter : public pcl_ros::PCLNodelet
{
public:
    // Constructor
	SpeckleFilter()
	  :	filter_speckle_(false)
	{
	}

    // Destructor
    ~SpeckleFilter()
    {
    	/// void
    }

    void onInit()
    {
    	PCLNodelet::onInit();
    	n_ = getNodeHandle();

		point_cloud_sub_ = n_.subscribe("point_cloud2", 1, &SpeckleFilter::PointCloudSubCallback, this);
		point_cloud_pub_ = n_.advertise<sensor_msgs::PointCloud2>("point_cloud2_filtered",1);
		n_.param("/filter_nodelets/filter_speckle", filter_speckle_, false);
		std::cout << "filter speckle: " << filter_speckle_ << std::endl;
    }

    void PointCloudSubCallback(const pcl::PointCloud<CPCPoint>::Ptr& pc)
        {
        	//ROS_INFO("PointCloudSubCallback");
    		pcl::PointCloud<CPCPoint>::Ptr pc_out(new pcl::PointCloud<CPCPoint>());
    		pc_out->points.resize(pc->points.size());
    		pc_out->header = pc->header;

    		cv::Mat xyz_mat_32F3 = cv::Mat(pc->height, pc->width, CV_32FC3);
    		cv::Mat intensity_mat_32F1 = cv::Mat(pc->height, pc->width, CV_32FC1);
    		float* f_ptr = 0;
    		float* i_ptr = 0;
    		int pc_msg_idx=0;
    		for (int row = 0; row < xyz_mat_32F3.rows; row++)
    		{
    			f_ptr = xyz_mat_32F3.ptr<float>(row);
    			i_ptr = intensity_mat_32F1.ptr<float>(row);
    			for (int col = 0; col < xyz_mat_32F3.cols; col++, pc_msg_idx++)
    			{
       				memcpy(&f_ptr[3*col], &pc->points[pc_msg_idx].x, 3*sizeof(float));
    				memcpy(&i_ptr[col], &pc->points[pc_msg_idx].intensity, sizeof(float));
    			}
    		}

    		if(filter_speckle_) FilterSpeckles(xyz_mat_32F3);
    		pc_msg_idx=0;
			for (int row = 0; row < xyz_mat_32F3.rows; row++)
			{
				f_ptr = xyz_mat_32F3.ptr<float>(row);
				for (int col = 0; col < xyz_mat_32F3.cols; col++, pc_msg_idx++)
				{
					memcpy(&pc->points[pc_msg_idx].x, &f_ptr[3*col], 3*sizeof(float));
				}
			}
    		point_cloud_pub_.publish(pc);

        }

    void FilterSpeckles(cv::Mat xyz_mat_32F3)
    {
    	cv::Mat buf;
    	ipa_Utils::FilterSpeckles(xyz_mat_32F3, 50,0.1, buf);
    	ROS_INFO("\tTime (FilterSpeckles) : %f", t.elapsed());
    	t.restart();
    }

    ros::NodeHandle n_;
    boost::timer t;

protected:
    ros::Subscriber point_cloud_sub_;
    ros::Publisher point_cloud_pub_;

    bool filter_speckle_;
};

PLUGINLIB_DECLARE_CLASS(cob_env_model, SpeckleFilter, SpeckleFilter, nodelet::Nodelet)


