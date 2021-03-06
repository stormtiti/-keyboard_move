/*!
 * Author: Yuanbo She yuanboshe@126.com
 * Group: ExBot http://blog.exbot.net
 *
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2014, ExBot.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <termios.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>

#include <ros/ros.h>
#include <signal.h>
#include <geometry_msgs/Twist.h>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "TimerDiff.h"

#define KEYCODE_W 0x77
#define KEYCODE_A 0x61
#define KEYCODE_S 0x73
#define KEYCODE_D 0x64
#define KEYCODE_E 0x65

#define KEYCODE_A_CAP 0x41
#define KEYCODE_D_CAP 0x44
#define KEYCODE_S_CAP 0x53
#define KEYCODE_W_CAP 0x57
#define KEYCODE_E_CAP 0x45


class AgvNode
{
    private:
        double walk_vel_;
        double run_vel_;
        double yaw_rate_;
        double yaw_rate_run_;

        geometry_msgs::Twist cmdvel_;

        double vx_tmp;
        double vw_tmp;

        ros::NodeHandle n_;
        ros::Publisher pub_;

    public:
        AgvNode()
        {
            pub_ = n_.advertise<geometry_msgs::Twist>("cmd_vel", 1);

            ros::NodeHandle n_private("~");
            n_private.param("walk_vel", walk_vel_, 0.5);
            n_private.param("run_vel", run_vel_, 1.0);
            n_private.param("yaw_rate", yaw_rate_, 1.0);
            n_private.param("yaw_rate_run", yaw_rate_run_, 1.5);

            vx_tmp = 0;
            vw_tmp = 0;
        }

        ~AgvNode() { }
        void keyboardLoop();

        void stopRobot()
        {
            cmdvel_.linear.x = 0.0;
            cmdvel_.angular.z = 0.0;
            pub_.publish(cmdvel_);
        }
};
int kfd = 0;
struct termios cooked,raw;
bool done;

//加速度限幅 current 当前值 acc加速度 set设定值
double CalAcc(double dt,double current,double acc,double set){
  double max_dv = 0;

  max_dv = acc*dt;

  if(fabs(set - current) >= max_dv){
    if(set > current){
      return (current + max_dv);
    }
    else{
      return (current - max_dv);
    }
  }
  return set;

}

void AgvNode::keyboardLoop()
{
    char c;
    double max_tv = walk_vel_;
    double max_rv = yaw_rate_;
    bool dirty = false;
    int speed = 0;
    int turn = 0;

    // get the console in raw mode
    tcgetattr(kfd, &cooked);
    memcpy(&raw, &cooked, sizeof(struct termios));
    raw.c_lflag &=~ (ICANON | ECHO);
    raw.c_cc[VEOL] = 1;
    raw.c_cc[VEOF] = 2;
    tcsetattr(kfd, TCSANOW, &raw);

    puts("Reading from keyboard");
    puts("Use WASD keys to control the robot");
    puts("Press Shift to move faster");

    struct pollfd ufd;
    ufd.fd = kfd;
    ufd.events = POLLIN;

    cTimerDiff time_diff;
    time_diff.Begin();

    for(;;)
    {
        boost::this_thread::interruption_point();

        // get the next event from the keyboard
        int num;

        time_diff.End();
        double dt = time_diff.GetTime() / 1e6;
        time_diff.Begin();

        if ((num = poll(&ufd, 1, 250)) < 0)
        {
            perror("poll():");
            return;
        }
        else if(num > 0)
        {
            if(read(kfd, &c, 1) < 0)
            {
                perror("read():");
                return;
            }
        }
        else
        {
            if (dirty == true)
            {
                //stopRobot();
                if(fabs(speed) > 0.00001){
                  max_tv = cmdvel_.linear.x / speed;
                }
                if(fabs(turn) > 0.00001){
                  max_rv = cmdvel_.angular.z / turn;
                }
                dirty = false;
            }

            continue;
        }



        switch(c)
        {
            case KEYCODE_W:
                max_tv = walk_vel_;
                speed = 1;
                //turn = 0;
                dirty = true;
                break;
            case KEYCODE_S:
                max_tv = walk_vel_;
                speed = -1;
                //turn = 0;
                dirty = true;
                break;
            case KEYCODE_A:
                max_rv = yaw_rate_;
                //speed = 0;
                turn = 1;
                dirty = true;
                break;
            case KEYCODE_D:
                max_rv = yaw_rate_;
                //speed = 0;
                turn = -1;
                dirty = true;
                break;

            case KEYCODE_W_CAP:
                max_tv = run_vel_;
                speed = 1;
                //turn = 0;
                dirty = true;
                break;
            case KEYCODE_S_CAP:
                max_tv = run_vel_;
                speed = -1;
                //turn = 0;
                dirty = true;
                break;
            case KEYCODE_A_CAP:
                max_rv = yaw_rate_run_;
                //speed = 0;
                turn = 1;
                dirty = true;
                break;
            case KEYCODE_D_CAP:
                max_rv = yaw_rate_run_;
                //speed = 0;
                turn = -1;
                dirty = true;
                break;
            case KEYCODE_E:
            case KEYCODE_E_CAP:
                max_rv = yaw_rate_;
                turn = 0;
                break;
            default:
                max_tv = walk_vel_;
                max_rv = yaw_rate_;
                speed = 0;
                turn = 0;
                dirty = false;
        }



        vx_tmp  = CalAcc(dt,vx_tmp,0.2,speed * max_tv);
        if(fabs(vx_tmp) < 0.1){
          cmdvel_.linear.x = 0;
        }else{
          cmdvel_.linear.x = vx_tmp;
        }
        vw_tmp = CalAcc(dt,vw_tmp,0.2,turn * max_rv);
        if(fabs(vw_tmp) < 0.1){
          cmdvel_.angular.z = 0;
        }
        else{
          cmdvel_.angular.z = vw_tmp;
        }
        pub_.publish(cmdvel_);
        ROS_INFO("sx:%f sw:%f",cmdvel_.linear.x,cmdvel_.angular.z);
    }
}


//ros::Publisher cmdVelPub;

void shutdown(int sig)
{
  //cmdVelPub.publish(geometry_msgs::Twist());
  ROS_INFO("exbot_example_move cpp ended!");
  ros::shutdown();
}

int main(int argc, char **argv)
{

  ros::init(argc, argv, "exbotxi_example_move");
  ros::NodeHandle node;
/*  std::string topic = "cmd_vel";
  ros::NodeHandle node;
  cmdVelPub = node.advertise<geometry_msgs::Twist>(topic, 1);

  geometry_msgs::Twist speed;
  speed.linear.x = 0.1; // 设置线速度为0.1m/s，正为前进，负为后退
  speed.angular.z = 0.0; // 设置角速度为0.4rad/s，正为左转，负为右转
 while(1){
  cmdVelPub.publish(speed); // 将刚才设置的指令发送给机器人
 }*/
  ros::Rate loopRate(40);
  signal(SIGINT, shutdown);
  ROS_INFO("exbot_example_move cpp start...");

  AgvNode agv;


  boost::thread t = boost::thread(boost::bind(&AgvNode::keyboardLoop,&agv));


  ros::spin();

 t.interrupt();
 t.join();
 agv.stopRobot();
 tcsetattr(kfd, TCSANOW, &cooked);
 return 0;

 // geometry_msgs::Twist speed; // 控制信号载体 Twist message
  while (ros::ok())
  {
    //speed.linear.x = 0.1; // 设置线速度为0.1m/s，正为前进，负为后退
    //speed.angular.z = 0.4; // 设置角速度为0.4rad/s，正为左转，负为右转
    //cmdVelPub.publish(speed); // 将刚才设置的指令发送给机器人

    loopRate.sleep();
  }

  return 0;
}
