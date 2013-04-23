/*
 * ir_scan_node.cpp
 *
 *  Created on: May 31, 2012
 *      Author: jorge
 */

#include <tf/tf.h>
#include <tf/transform_listener.h>

#include <sensor_msgs/LaserScan.h>

//#include <yaml-cpp/yaml.h>

#include "waiterbot/common.hpp"

#include "waiterbot/ir_scan_node.hpp"

namespace waiterbot
{

std::vector<double> kkk;
IrScanNode::IrScanNode()
{
}

IrScanNode::~IrScanNode()
{
}

void IrScanNode::sonarsMsgCB(const arduino_resources::Rangers::ConstPtr& msg)
{
  double RADIUS = 0.155;
  double FREQUENCY = 20.0;

  sensor_msgs::LaserScan scan;
  scan.header = msg->header;
  scan.header.frame_id = "ir_link";
  scan.ranges.resize(msg->ranges.size(), 0.0);
  scan.intensities.resize(msg->ranges.size(), 0.0);
//std algo   scan.intensities = msg->ranges;

  scan.angle_min = -M_PI/2.0;// - 0.1;
  scan.angle_max = +M_PI/2.0;
  scan.angle_increment = M_PI/10; //(18.0*M_PI)/180.0;
//  scan.time_increment
  scan.scan_time = 1/FREQUENCY;
  scan.range_min = 0.10 + RADIUS;
  scan.range_max = 11.00 + RADIUS;

  for (unsigned int i = 0; i < msg->ranges.size(); i++)
  {
    double v = msg->ranges[i]; // no filter by now
    double r = 1.06545479706866e-15*pow(v, 6) - 2.59219822235705e-12*pow(v, 5) + 2.52095247302813e-09*pow(v, 4)
             - 1.25091335895759e-06*pow(v, 3) + 0.000334991560873548*pow(v, 2) - 0.0469975280676629*v + 3.01895762047759;

    // identify the position of its reading in the incoming message data
    int idx = sonars[i].idx_buffer;
    // store reading a timestamp in the proper place of its internal buffer
//    sonars[i].readings[idx] = r;
////    sonars[i].t_stamps[idx] = msg->header.stamp;
//    // and increment the internal buffer index
//    sonars[i].idx_buffer  = (sonars[i].idx_buffer + 1) % sonar_buffer_size;


//    FILE* f = fopen("COV.m", "w");
//fprintf(file, "%f\t", r);
//    double r = 7.17062444495719e-15*pow(v, 6) - 1.23976199902831e-11*pow(v, 5) + 8.66526083489264e-09*pow(v, 4)
//             - 3.15513738809603e-06*pow(v, 3) + 0.000639205288260282*pow(v, 2) - 0.0704748189962500*v + 3.69769525786646;

//    prevDist = 0;
//
    // estimate - prediction
    sonars[i].P = sonars[i].P + sonars[i].Q;

    // correction
   // double E = H * P * H';

    double z = r - sonars[i].last_range;
    double Z = sonars[i].R + sonars[i].P;

    double K = sonars[i].P/Z;

    double distanceKF = sonars[i].last_range + K*z;
    sonars[i].P = sonars[i].P - K*sonars[i].P;

    // collect data
    sonars[i].last_range = distanceKF;

    sonars[i].voltage[idx]  = v;
    sonars[i].distance[idx] = r;
    sonars[i].filt_err[idx] = distanceKF - r;

//    sonars[i].t_stamps[idx] = msg->header.stamp;
    // and increment the internal buffer index
    sonars[i].idx_buffer  = (sonars[i].idx_buffer + 1) % sonar_buffer_size;

//    int idx1 = sonars[i].idx_buffer;
//    int idx2 = (idx + 1) % sonar_buffer_size;
    //idx--;
    //if (idx < 0) idx =  sonar_buffer_size-1;
    //    return distanceKF;

    // inverse order, as for laser scan first value correspond to A10
    //scan.intensities[msg->ranges.size() - (i + 1)] = r + RADIUS;//(r <= 110.6)?(r + RADIUS):6.0;

   // double var = tk::variance(sonars[i].distance);

    // Filter by checking accumulated difference between filtered and unfiltered ranges
    double acc_filt_err = 0.0;
    for (unsigned int j = 0; j < sonar_buffer_size; j++)
    {
      acc_filt_err += std::abs(sonars[i].filt_err[j]);
    }
    double avg_filt_err = acc_filt_err/sonar_buffer_size;

    // Filter by checking accumulated difference between consecutive raw readings
    double acc_abs_diff = 0.0;
    double acc_sgn_diff = 0.0;

    for (unsigned int j = 1; j < sonar_buffer_size; j++)
    {
      int idx1 = (idx  + j) % sonar_buffer_size;
      int idx2 = (idx1 + 1) % sonar_buffer_size;

      double diff = sonars[i].voltage[idx2] - sonars[i].voltage[idx1];
      acc_abs_diff += std::abs(diff);
      acc_sgn_diff +=          diff;
    }

    double avg_abs_diff = acc_abs_diff/sonar_buffer_size - 1;
    double avg_sgn_diff = acc_sgn_diff/sonar_buffer_size - 1;

 //   fprintf(f, "  %f  %f  %f  %f\n", avg_abs_diff, avg_sgn_diff, avg_sgn_diff/avg_abs_diff, tk::mean(sonars[i].distance));

    // Filter by adaptive variance threshold
    double var_threshold = max_covariance*(0.5 + std::pow(std::max(0.0, std::min(1.0, (distanceKF - closest_rejected)/(farthest_contact - closest_rejected))), 2));

    if (tk::variance(sonars[i].distance) > var_threshold)
      sonars[i].noisy_read = std::min(sonars[i].noisy_read + 1, + noisy_readings);
    else
      sonars[i].noisy_read = std::max(sonars[i].noisy_read - 1, - noisy_readings);

    scan.ranges[msg->ranges.size() - (i + 1)] = ((distanceKF <= farthest_contact) && (sonars[i].noisy_read <= 0.0)) || (distanceKF < closest_rejected)?(distanceKF + RADIUS):6.0;
//    scan.intensities[msg->ranges.size() - (i + 1)] = r;//avg_abs_diff;//tk::variance(sonars[i].distance);//r + RADIUS;//(r <= 110.6)?(r + RADIUS):6.0;



    if (i == 5)
    {
      if (avg_abs_diff >= max_mean_error)
        scan.intensities[0] = 0.55;
      else
        scan.intensities[0] = 0.0;

      if (avg_filt_err >= max_mean_error/100.0)
        scan.intensities[1] = 0.5;
      else
        scan.intensities[1] = 0.0;

      if (tk::variance(sonars[i].distance) >= max_covariance)
        scan.intensities[2] = 0.45;
      else
        scan.intensities[2] = 0.0;

      if (tk::variance(sonars[i].voltage) >= max_mean_error*10.0)
        scan.intensities[3] = 0.4;
      else
        scan.intensities[3] = 0.0;

      double var_threshold = max_covariance*(0.5 + std::pow(std::max(0.0, std::min(1.0, (distanceKF - closest_rejected)/(farthest_contact - closest_rejected))), 2));

      if (tk::variance(sonars[i].distance) >=  var_threshold)
          scan.intensities[4] = 0.3;
      else
        scan.intensities[4] = 0.0;

      scan.intensities[msg->ranges.size() - (i + 1)] = r+ RADIUS;//avg_abs_diff;//tk::variance(sonars[i].distance);//r + RADIUS;//(r <= 110.6)?(r + RADIUS):6.0;

      scan.intensities[6] = var_threshold;
      scan.intensities[7] = tk::variance(sonars[i].distance);
      scan.intensities[8] = sonars[i].noisy_read;

//      fprintf(f, "%f %f %f %f %f %f %f %f %d %d\n", v, r, distanceKF, tk::variance(sonars[i].distance), tk::std_dev(sonars[i].distance), avg_filt_err, avg_abs_diff, avg_sgn_diff,
//              avg_filt_err >= max_covariance/100.0, avg_abs_diff >= max_covariance);
    }
  }

//  if (sonars[0].idx_buffer == 0)
//  {
//    for (unsigned int i = 0; i < msg->ranges.size(); i++) {
//
//      double mean = 0;
//      for (unsigned int j = 0; j < sonar_buffer_size; j++)
//        mean += sonars[i].readings[j];
//
//      double v = mean/sonar_buffer_size;//     msg->ranges[i]; // no filter by now
//      double r = 1.06545479706866e-15*pow(v, 6) - 2.59219822235705e-12*pow(v, 5) + 2.52095247302813e-09*pow(v, 4)
//               - 1.25091335895759e-06*pow(v, 3) + 0.000334991560873548*pow(v, 2) - 0.0469975280676629*v + 3.01895762047759;
//
//      scan.ranges[i] = r;
//      scan.intensities[i] = v;
//    }
    pcloud_pub.publish(scan);
//  }
}
/*
void operator >> (const YAML::Node& node, IrScanNode::Sonar& sonar) {
  node["name"]        >> sonar.name;
  node["frame_id"]    >> sonar.frame_id;
  node["index_fired"] >> sonar.idx_fired;
  node["index_value"] >> sonar.idx_value;
}

bool IrScanNode::loadSonarsCfg(std::string path)
{
  std::ifstream ifs(path.c_str(), std::ifstream::in);
  if (ifs.good() == false) {
    ROS_ERROR("Unable to read sonars configuration file: %s", path.c_str());
    return false;
  }

  bool result = true;

  try {
    YAML::Parser parser(ifs);

    YAML::Node doc;
    parser.GetNextDocument(doc);

    doc["sonar_buffer_size"] >> sonar_buffer_size;
    doc["sonar_max_reading"] >> sonar_max_reading;
    doc["no_contact_range"]  >> no_contact_range;

    for (unsigned int i = 0; i < doc["sonars"].size(); i++) {
      Sonar sonar;
      doc["sonars"][i] >> sonar;
      sonars.push_back(sonar);
    }
  } catch(YAML::ParserException& e) {
    ROS_ERROR("Sonars configuration file parse failed: %s", e.what());
    result = false;
  } catch(YAML::RepresentationException& e) {
    ROS_ERROR("Sonars configuration file wrong format: %s", e.what());
    result = false;
  }

  ifs.close();
  return result;
}
*/
int IrScanNode::init(ros::NodeHandle& nh)
{
  // Parameters
  std::string default_frame("/base_link");
  std::string default_file("sonars_config.yaml");
  nh.param("sonars_to_pc/frequency", frequency, 20.0);
  nh.param("sonars_to_pc/pcloud_frame_id", pcloud_frame, default_frame);
  nh.param("sonars_to_pc/sonars_cfg_file", sonars_cfg_file, default_file);


  nh.param("ir_to_laserscan/max_mean_error",   max_mean_error,  0.01);
  nh.param("ir_to_laserscan/max_covariance",   max_covariance,  0.01);
  nh.param("ir_to_laserscan/readings_buffer",  readings_buffer,    5);
  nh.param("ir_to_laserscan/farthest_contact", farthest_contact, 0.8);
  nh.param("ir_to_laserscan/closest_rejected", closest_rejected, 0.4);
  nh.param("ir_to_laserscan/noisy_readings", noisy_readings, 10);

//  if (loadSonarsCfg(sonars_cfg_file) == false)
  //  return ERROR;

  sonar_buffer_size = readings_buffer;
  sonars.resize(11, Sonar());
  const ros::Time       t0(0);
  const ros::Duration   d4(4);
  tf::StampedTransform  stf;
  tf::TransformListener tfl;

  for (unsigned int i = 0; i < sonars.size(); i++)
  {
    // Allocate sonar readings and timestamps internal buffers
    sonars[i].idx_buffer = 0;
    sonars[i].noisy_read = 0;
    sonars[i].last_range = farthest_contact;
    sonars[i].voltage.resize(sonar_buffer_size, 0);
    sonars[i].distance.resize(sonar_buffer_size, 0);
    sonars[i].filt_err.resize(sonar_buffer_size, 0);
    sonars[i].t_stamps.resize(sonar_buffer_size, t0);


    sonars[i].Q = 0.001;
    sonars[i].R = 0.025; //0.0288;
    sonars[i].P = sonars[i].R;

//    ROS_DEBUG("%s sonar configured with frame id %s",
  //            sonars[i].name.c_str(), sonars[i].frame_id.c_str());
  }

//  if (sonars.size() == 0)
//  {
//    ROS_ERROR("No sonars configured; check sonars configuration yaml file content");
//    return ERROR;
//  }

//  ROS_DEBUG("Sonars configuration file  %s successfully parsed", sonars_cfg_file.c_str());

//  const ros::Time       t0(0);
//  const ros::Duration   d4(4);
//  tf::StampedTransform  stf;
//  tf::TransformListener tfl;
//
//  for (unsigned int i = 0; i < sonars.size(); i++)
//  {
//    // For each sonar, get the static tf: sonars origin ->  current sonar
//    // To use ros::Time(0) means that we'll take the latest available tf, what's fine for static
//    // transforms. Transform should be available immediately, but anyway we wait up to 4 seconds
//    tf::StampedTransform sonar_tf;
//    try {
//      tfl.waitForTransform(pcloud_frame, sonars[i].frame_id, t0, d4);
//      tfl.lookupTransform(pcloud_frame, sonars[i].frame_id, t0, stf);
//    }
//    catch (tf::TransformException e) {
//      ROS_FATAL("Get %s -> %s tf failed: %s", sonars[i].frame_id.c_str(), pcloud_frame.c_str(),
//                e.what());
//      return ERROR;
//    }
//    sonars[i].transform = (tf::Transform)stf;
//
//    // Allocate sonar readings and timestamps internal buffers
//    sonars[i].idx_buffer = 0;
//    sonars[i].readings.resize(sonar_buffer_size, sonar_max_reading);
//    sonars[i].t_stamps.resize(sonar_buffer_size, t0);
//
//    ROS_DEBUG("%s sonar configured with frame id %s",
//              sonars[i].name.c_str(), sonars[i].frame_id.c_str());
//  }

  // Publishers and subscriptors
  sonars_sub = nh.subscribe("rangers_data", 1, &IrScanNode::sonarsMsgCB, this);
  pcloud_pub = nh.advertise< sensor_msgs::LaserScan>("ir_scan", 1);

  ROS_INFO("Sonars to pointcloud node successfully initialized with %lu sonars", sonars.size());

  return 0;
}
/*
int IrScanNode::spin() {
  long long int iter = 0;

  pcl::PointCloud<pcl::PointXYZ> pcloud;
  pcloud.header.frame_id = pcloud_frame;

  pcloud.points.resize(sonars.size(), pcl::PointXYZ());

  ros::Rate r(frequency); // sonar messages come at 40 hz, so the three sets get ready at 13.3 hz
  while (ros::ok())
  {
    double stamp = 0.0;

    // The 2nd loop exit condition allows callbacks to fill the buffers before publish any data

    for (unsigned int i = 0; (i < sonars.size()) && (iter > 3*sonar_buffer_size); i++) {
      // For each sonar: prepare a beam with the measured distance as x-axis and 0 on the others,
      // and transform according to the current sonar frame. Note that we store readings for each
      // sonar in an internal buffer to apply a median filter, instead of returning raw readings.
      // We cannot send the 255 maximum distance provided by the sonars because it will probably
      // be below the obstacle_range costmap_2d parameter, and so included in the costmap as an
      // obstacle. See http://ros.org/wiki/costmap_2d#Global_costmap_parameters for details
      uint8_t dist = median(sonars[i].readings);
      tf::Vector3 beam_sonar((dist == sonar_max_reading)?no_contact_range:dist/100.0, 0.0, 0.0);
      tf::Vector3 beam_pcloud = sonars[i].transform * beam_sonar;

      pcloud.points[i].x = beam_pcloud.x();
      pcloud.points[i].y = beam_pcloud.y();
      pcloud.points[i].z = beam_pcloud.z();

      // For the timestamp, we calculate the average of all sonars, assuming that the median value
      // is the middle element in the historical list of readings.
      int expected_median = (sonar_buffer_size + (sonars[i].idx_buffer - 1 - sonar_buffer_size/2));
      stamp += sonars[i].t_stamps[expected_median%sonar_buffer_size].toSec();
    }

    pcloud.header.stamp = ros::Time(stamp/sonars.size());
    pcloud_pub.publish(pcloud);

    iter++;

    ros::spinOnce();
    r.sleep();
  }

  return OK;
}
*/
} // namespace waiterbot

int main(int argc, char **argv)
{
  ros::init(argc, argv, "sonars_to_pc");

  ros::NodeHandle nh;

  waiterbot::IrScanNode node;
  if (node.init(nh) != 0)
    return -1;

  ros::spin();

  return 0;
}