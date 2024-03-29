#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"

//external reference available in https://kluge.in-chemnitz.de/opensource/spline/spline.h
#include "spline.h"  

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

//---------------------------------------------------------------------------------------------  
  //start in lane 1
  int lane = 1;
  
  // Have a reference velocity to target
  double ref_vel = 0.0; //mph
//--------------------------------------------------------------------------------------------- 
  
  
  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy,&lane,&ref_vel]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];      
          
          
//---------------------------------------------------------------------------------------------           
          int prev_size = previous_path_x.size(); 
          
          if(prev_size > 0 )
          {
            car_s = end_path_s;
          }
          bool too_close = false;   
         
          float fr_left_lane = 0.5;
          float rr_left_lane = 0.5;
          
          float fr_middle_lane = 0.5;
          float rr_middle_lane = 0.5;                     
                    
          float fr_right_lane = 0.5;
          float rr_right_lane = 0.5;             
          
          int car_ahead_distance = 30;
          int red_zone_check_distance = 30;  
          int safety_distance = 20;
          
          double car_speed_ahead = -1;
          double nearest_car_ahead_position = car_ahead_distance;
          
          // Detecting car ahead 
          for (int i = 0;  i < sensor_fusion.size(); i++)
          {                       
            float d = sensor_fusion[i][6];                 
           
            //car is in my lane
            if ((d < ( 2+4 * lane + 2)) && (d > (2+4 * lane - 2)))
            {            
              double vx = sensor_fusion[i][3];
              double vy = sensor_fusion[i][4];
              double check_speed = sqrt ( vx*vx + vy*vy);
              double check_car_s = sensor_fusion[i][5];     
              
              check_car_s += ((double)prev_size* .02 * check_speed);
              //car position and speed on the maneuvering time
              
              if ((check_car_s > car_s) && ((check_car_s - car_s) < car_ahead_distance ))
              {          
                too_close = true; 
                
                if ( check_car_s < nearest_car_ahead_position)
                {
                  nearest_car_ahead_position = check_car_s;
                  car_speed_ahead = check_speed;
                }
              }
            }
          }          
          
          
          double left_lane_speed = 1;
          double right_lane_speed = 0;
          double car_at_middle_front_position = 200;
          
          // getting  lanes status
          for (int i = 0;  i < sensor_fusion.size(); i++)
          {           
            double vx = sensor_fusion[i][3];
            double vy = sensor_fusion[i][4];
            double check_speed = sqrt ( vx*vx + vy*vy);
            double check_car_s = sensor_fusion[i][5];

            float d = sensor_fusion[i][6];            
            
            check_car_s += ((double)prev_size* .02 * check_speed);
            // if using previous points can project s value out
            // check s values greater than mine and s gap            
         
            
            // checking left lane gap
            if ((d < 4) && (check_car_s >= car_s ))
            {                         
              if(check_speed > left_lane_speed )
              {
                left_lane_speed = check_speed;
              }              
              if (check_car_s - car_s <= red_zone_check_distance) // checking car at front.
              { 
                fr_left_lane = 0;
              }
              else if ((check_speed > (car_speed*.44704)) && (check_car_s - car_s > safety_distance) )
              {
                fr_left_lane = 0.5;
              }
            }
            else if ((d < 4) && ( car_s >= check_car_s )) 
            {  
              if (car_s - check_car_s <= red_zone_check_distance) // checking car at rear.
              {
                rr_left_lane = 0;
              }
              else if (((car_speed*.44704) > check_speed) && (car_s - check_car_s > safety_distance) )
              {
                rr_left_lane = 0.5;
              }
            }
            
            
            // checking middle lane
            if ((d > 4) && (d < 8) && (check_car_s >= car_s ))
            {                                    
              if(check_car_s < car_at_middle_front_position) 
              {
                car_at_middle_front_position = check_car_s; 
              }
              if (check_car_s - car_s <= red_zone_check_distance) // checking car at front.
              { 
                fr_middle_lane = 0;
              }
              else if ((check_speed > (car_speed*.44704)) && (check_car_s - car_s > safety_distance) )
              {
                fr_middle_lane = 0.5;
              }
            }
            
            else if ((d > 4) && (d < 8) && ( car_s >= check_car_s )) 
            {  
              if (car_s - check_car_s <= red_zone_check_distance) // checking car at rear.
              {
                rr_middle_lane = 0;
              }
              else if (((car_speed*.44704) > check_speed) && (car_s - check_car_s >= safety_distance) )
              {
                rr_middle_lane = 0.5;
              }
            }
            
            // checking right lane
            if ((d > 8) && (check_car_s >= car_s ))
            {                         
              if(check_speed > right_lane_speed )
              {
                right_lane_speed = check_speed;
              }  
             
              if (check_car_s - car_s <= red_zone_check_distance) // checking car at front.
              { 
                fr_right_lane = 0;
              }
              else if ((check_speed > (car_speed*.44704)) && (check_car_s - car_s > safety_distance) )
              {
                fr_right_lane = 0.5;
              }
            }
            else if ((d > 8) && ( car_s >= check_car_s )) 
            {  
              if (car_s - check_car_s <= red_zone_check_distance) // checking car at rear.
              {
                rr_right_lane = 0;
              }
              else if (((car_speed*.44704) > check_speed) && (car_s - check_car_s > safety_distance) )
              {
                rr_right_lane = 0.5;
              }
            }         
          }
          
          float left_lane_gap = fr_left_lane + rr_left_lane; // free   
          float middle_lane_gap = fr_middle_lane + rr_middle_lane; // free 
          float right_lane_gap = fr_right_lane + rr_right_lane; // free                       
          
          // car behavior
          
          if (too_close == true)
          {
            ref_vel -= .224;
            if ((lane == 0) && (middle_lane_gap == 1.0))
            {
              lane++ ;
            }
            else if ((lane == 1) && (left_lane_gap == 1.0) && (right_lane_gap == 1.0))
            {
              if (left_lane_speed < right_lane_speed)
              {
                lane ++;
              }
              else
              {
                lane --;
              }
            }                 
            else if ((lane == 1) && (left_lane_gap == 1.0) && (right_lane_gap == 0.0))
            {
              lane --;
            }  
            else if ((lane == 1) && (left_lane_gap == 0.0) && (right_lane_gap == 1.0))
            {
              lane ++;
            }              
        
            else if ((lane == 2) && (middle_lane_gap == 1.0))
            {
              lane-- ;
            }
          }
          else if (ref_vel < 49.5)
          {
            ref_vel += .224;               
          }
     
//---------------------------------------------------------------------------------------------           
          json msgJson;

          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
//------------------------------------------------------------------------------------------------------
            
          // create a list of widely spaced (x,y) waypoints, evenly spaced at 30m
          // later we will interpolate theses waypoints with a spline and fill it in with more points that control speed.          
          
          vector<double> ptsx;
          vector<double> ptsy;
          
          //reference x,y and yaw states
          //either we will reference the starting point as where the car is or at the previous path end point
          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);
          
          //if previous size is almost empty, use the car as starting reference
          if (prev_size < 2)
          {
            //Use two points that make the path tangent to the car
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);
            
            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);
            
            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          else
          {
            //redefine reference state as previous path end point
            ref_x = previous_path_x[prev_size-1];
            ref_y = previous_path_y[prev_size-1];
            
            double ref_x_prev = previous_path_x[prev_size-2];
            double ref_y_prev = previous_path_y[prev_size-2];
            ref_yaw = atan2(ref_y - ref_y_prev,ref_x - ref_x_prev);
            
            // Use two points that make the path tangent to the previous Path's end point
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);
            
            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);
          }
          
          //In frenet add evenly 30m spaced points ahead of the starting reference
          vector<double>next_wp0 = getXY(car_s+30,(2+4 * lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);
          vector<double>next_wp1 = getXY(car_s+60,(2+4 * lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);            
          vector<double>next_wp2 = getXY(car_s+90,(2+4 * lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);  
          
          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);          
          
          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);     
          
          for (int i=0; i < ptsx.size(); i++)
          {
            // shift car reference angle to 0 degrees
            
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;
            
            ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin (0 - ref_yaw));
            ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos (0 - ref_yaw));
          }
          
          //create a spline
          tk:: spline s;
          
          // set (x,y) points to the spline
          s.set_points(ptsx,ptsy);
          
          //Define the actual (x,y) points we will use for the planner
          vector<double> next_x_vals;
          vector<double> next_y_vals;
          
          //Start with all of the previous path points from last time
          for (int i = 0; i < previous_path_x.size(); i++)
          {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);          
          }
          // Calculate how to break up spline points so that we travel at our desired reference velocity
          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt((target_x)*(target_x) + (target_y)*(target_y));
          
          double x_add_on = 0;
          
          //Fill up the rest of our path planner after filling it with previous points , here we will always output 50 points
          for (int i = 1; i <= 50 - previous_path_x.size(); i++)
          {
            double N = (target_dist/ (.02 * ref_vel/2.24));
            double x_point = x_add_on + (target_x) / N;
            double y_point = s(x_point);
            
            x_add_on = x_point;
            
            double x_ref = x_point;
            double y_ref = y_point;
            
            // rotate back to normal after rotating it earlier
            x_point = (x_ref * cos (ref_yaw) - y_ref * sin (ref_yaw));
            y_point = (x_ref * sin (ref_yaw) + y_ref * cos (ref_yaw));
            
            x_point += ref_x;
            y_point += ref_y;
            
            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);            
          }
       
          
//------------------------------------------------------------------------------------------------------
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}