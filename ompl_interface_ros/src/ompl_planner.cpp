/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2011, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan, Sachin Chitta */

#include "ompl_interface_ros/ompl_interface_ros.h"
#include "planning_scene_monitor/planning_scene_monitor.h"
#include <tf/transform_listener.h>
#include <visualization_msgs/MarkerArray.h>

static const std::string PLANNER_NODE_NAME="ompl_planning";          // name of node
static const std::string PLANNER_SERVICE_NAME="plan_kinematic_path"; // name of the advertised service (within the ~ namespace)
static const std::string BENCHMARK_SERVICE_NAME="benchmark_planning_problem"; // name of the advertised service (within the ~ namespace)
static const std::string ROBOT_DESCRIPTION="robot_description";      // name of the robot description (a param name, so it can be changed externally)

class OMPLPlannerService
{
public:
  
  OMPLPlannerService(planning_scene_monitor::PlanningSceneMonitor &psm) : nh_("~"), psm_(psm), ompl_interface_(psm.getPlanningScene()->getKinematicModel())
  {
    plan_service_ = nh_.advertiseService(PLANNER_SERVICE_NAME, &OMPLPlannerService::computePlan, this);
    benchmark_service_ = nh_.advertiseService(BENCHMARK_SERVICE_NAME, &OMPLPlannerService::computeBenchmark, this);  
    pub_markers_ = nh_.advertise<visualization_msgs::MarkerArray>("visualization_marker_array", 5);
  }
  
  bool computePlan(moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res)
  {
    ROS_INFO("Received new planning request...");
    bool result = ompl_interface_.solve(psm_.getPlanningScene(), req, res);
    displayPlannerData("r_wrist_roll_link");
    return result;
  }
  
  void displayPlannerData(const std::string &link_name)
  {    
    const ompl_interface::PlanningConfigurationPtr &pc = ompl_interface_.getLastPlanningConfiguration();
    if (pc)
    {
      const ompl::base::PlannerData &pd = pc->getOMPLSimpleSetup().getPlannerData();
      planning_models::KinematicState kstate = psm_.getPlanningScene()->getCurrentState();  
      visualization_msgs::MarkerArray arr; 
      std_msgs::ColorRGBA color;
      color.r = 1.0f;
      color.g = 0.0f;
      color.b = 0.0f;
      color.a = 1.0f;
      for (std::size_t i = 0 ; i < pd.states.size() ; ++i)
      {
        pc->getKMStateSpace().copyToKinematicState(kstate, pd.states[i]);
        kstate.getJointStateGroup(pc->getJointModelGroupName())->updateLinkTransforms();
        const Eigen::Vector3d &pos = kstate.getLinkState(link_name)->getGlobalLinkTransform().translation();
        
        visualization_msgs::Marker mk;
        mk.header.stamp = ros::Time::now();
        mk.header.frame_id = psm_.getPlanningScene()->getPlanningFrame();
        mk.ns = "planner_data";
        mk.id = i;
        mk.type = visualization_msgs::Marker::SPHERE;
        mk.action = visualization_msgs::Marker::ADD;
        mk.pose.position.x = pos.x();
        mk.pose.position.y = pos.y();
        mk.pose.position.z = pos.z();
        mk.pose.orientation.w = 1.0;
        mk.scale.x = mk.scale.y = mk.scale.z = 0.035;
        mk.color = color;
        mk.lifetime = ros::Duration(10.0);
        arr.markers.push_back(mk);
      }
      pub_markers_.publish(arr);
    }
  }
  
  bool computeBenchmark(moveit_msgs::ComputePlanningBenchmark::Request &req, moveit_msgs::ComputePlanningBenchmark::Response &res)
  {
    ROS_INFO("Received new benchmark request...");
    return ompl_interface_.benchmark(psm_.getPlanningScene(), req, res);
  }
  
  void status(void)
  {
    ompl_interface_.printStatus();
    ROS_INFO("Responding to planning and bechmark requests");
  }
  
private:
  
  ros::NodeHandle                               nh_;
  planning_scene_monitor::PlanningSceneMonitor &psm_;  
  ompl_interface_ros::OMPLInterfaceROS          ompl_interface_;
  ros::ServiceServer                            plan_service_;
  ros::ServiceServer                            benchmark_service_;  
  ros::ServiceServer                            display_states_service_;
  ros::Publisher                                pub_markers_;
};

int main(int argc, char **argv)
{
  ros::init(argc, argv, PLANNER_NODE_NAME);
  
  ros::AsyncSpinner spinner(1);
  spinner.start();
  
  tf::TransformListener tf;
  planning_scene_monitor::PlanningSceneMonitor psm(ROBOT_DESCRIPTION, &tf);
  if (psm.getPlanningScene()->isConfigured())
  {
    psm.startWorldGeometryMonitor();
    psm.startSceneMonitor();
    psm.startStateMonitor();
    
    OMPLPlannerService pservice(psm);
    pservice.status();
    ros::waitForShutdown();
  }
  else
    ROS_ERROR("Planning scene not configured");
  
  return 0;
}
