/**
 * @file junction_data.cpp
 * @author Tiago Almeida (tm.almeida@ua.pt)
 * @brief The junction of the 2 images treated with painted area is made here!
 * @version 0.1
 * @date 2019-04-02
 *
 * @copyright Copyright (c) 2019
 *
 */

#include <cv_bridge/cv_bridge.h>
#include <geometry_msgs/Point32.h>
#include <image_transport/image_transport.h>
#include <lane_detector/fitting.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include "ros/ros.h"

using namespace std;
using namespace ros;
using namespace cv;

ros::Publisher merged_image;
ros::Publisher intersect_image;
ros::Publisher non_intersect_image;
ros::Publisher prob_map;

class junction_data
{
public:
  cv_bridge::CvImagePtr current_image_alg1;
  cv_bridge::CvImagePtr current_image_alg2;

  void imageAlg1(const sensor_msgs::ImageConstPtr &img1)
  {
    try
    {
      current_image_alg1 = cv_bridge::toCvCopy(img1, sensor_msgs::image_encodings::BGR8);
      mergedImage();
      diffImage();
    }
    catch (cv_bridge::Exception &e)
    {
      ROS_ERROR("Could not convert from '%s' to 'bgr8'.", img1->encoding.c_str());
    }
  }

  void imageAlg2(const sensor_msgs::ImageConstPtr &img2)
  {
    try
    {
      current_image_alg2 = cv_bridge::toCvCopy(img2, sensor_msgs::image_encodings::BGR8);
      mergedImage();
      diffImage();
    }
    catch (cv_bridge::Exception &e)
    {
      ROS_ERROR("Could not convert from '%s' to 'bgr8'.", img2->encoding.c_str());
    }
  }

  void mergedImage()
  {
    if (current_image_alg1 && current_image_alg2)
    {
      Mat img_alg1 = current_image_alg1->image;
      Mat img_alg2 = current_image_alg2->image;
      Mat img_summed;

      add(img_alg1, img_alg2, img_summed);
      auto img_final_summed = cv_bridge::CvImage{current_image_alg1->header, "bgr8", img_summed};
      merged_image.publish(img_final_summed);
    }
  }

  void diffImage()
  {
    if (current_image_alg1 && current_image_alg2)
    {
      Mat img_diff;
      Mat img_ninter;
      Mat img_alg1 = current_image_alg1->image;
      Mat img_alg2 = current_image_alg2->image;
      int i, j = 0;
      cvtColor(img_alg1, img_alg1, CV_BGR2GRAY);
      cvtColor(img_alg2, img_alg2, CV_BGR2GRAY);
      threshold(img_alg1, img_alg1, 0, 255, THRESH_BINARY | THRESH_OTSU);
      threshold(img_alg2, img_alg2, 0, 255, THRESH_BINARY | THRESH_OTSU);
      bitwise_and(img_alg1, img_alg2, img_diff);
       
      bitwise_xor(img_alg1,img_alg2,img_ninter);
      auto img_final_diff = cv_bridge::CvImage{current_image_alg1->header, "mono8", img_diff};
      auto img_final_nao_int=cv_bridge::CvImage{current_image_alg1->header, "mono8", img_ninter};
      intersect_image.publish(img_final_diff);
      non_intersect_image.publish(img_final_nao_int);
      probabilitiesMapImage(img_diff,img_ninter);
    }
  }

  void probabilitiesMapImage(Mat &input, Mat &input2)
  {
    if (current_image_alg1 && current_image_alg2)
    {
      Mat kernel;
      Mat img_filt;
      Mat img_final;
      Point anchor;
      double delta;
      int ddepth;
      int kernel_size;
      int x = 0;
      int y = 0;
      float pix_cinzentosf = 0;
      int value_filter = 0;
      float prob = 0.0;
      float prob_non_intersect=0.0;
      int thresh_non_intersect=1; // valore acrescentado para termos a probabilidade das secções que nao se intersectam 
      kernel_size = 25;
      input.convertTo(input, CV_32F);
      input2.convertTo(input2, CV_32F);
      input2=input2/(float)(kernel_size+thresh_non_intersect);

      kernel = Mat::ones(kernel_size, kernel_size, CV_32F) /(float)(kernel_size * kernel_size);

      /// Initialize arguments for the filter
      anchor = Point(-1, -1);
      delta = 0;
      ddepth = -1;

      filter2D(input, img_filt, ddepth, kernel, anchor, delta, BORDER_DEFAULT);
      img_final = Mat::zeros(input.rows, input.cols, CV_8UC1);
      img_filt=img_filt+input2;

      for (x = 0; x < input.rows; x++)
      {
        for (y = 0; y < input.cols; y++)
        {
          value_filter = img_filt.at<uchar>(x, y);
          prob = value_filter / (float)255;
          pix_cinzentosf = prob * (float)255;
          img_final.at<uchar>(x, y) = (int)pix_cinzentosf;

          if (input2.at<uchar>(x, y)!=0)
          {
            prob_non_intersect=input2.at<uchar>(x, y)/(float) 255;
          }

          // if (value_filter != 0)
          // {
          //   cout << "value filter= " << value_filter << endl;
          //   cout << "Probabilidade " << prob << endl;
          // }
        }
      }
      
      img_filt.convertTo(img_filt,CV_8UC1);
      auto img_final_map = cv_bridge::CvImage{current_image_alg1->header, "mono8", img_filt};
      prob_map.publish(img_final_map);
    }
  }
};

int main(int argc, char **argv)
{
  ros::init(argc, argv, "junction_data");

  ros::NodeHandle n;
  image_transport::ImageTransport it(n);
  junction_data data;

  image_transport::Subscriber sub_img1 = it.subscribe("data_treatment/final", 10, &junction_data::imageAlg1, &data);
  image_transport::Subscriber sub_img2 = it.subscribe("data_treatment2/final", 10, &junction_data::imageAlg2, &data);
  merged_image = n.advertise<sensor_msgs::Image>("junction_data/summed_img", 10);
  intersect_image = n.advertise<sensor_msgs::Image>("junction_data/diff_img", 10);
  non_intersect_image=n.advertise<sensor_msgs::Image>("junction_data/nonintersect_img", 10);
  prob_map = n.advertise<sensor_msgs::Image>("junction_data/prob_map", 10);
  ros::spin();

  return 0;
}
