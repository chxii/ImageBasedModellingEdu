//
// Created by caoqi on 2018/9/5.
//
#include <iostream>
#include "math/vector.h"
class Camera{

public:

    // constructor
    Camera(){

        // 采用归一化坐标，不考虑图像尺寸
        c_[0]=c_[1] = 0.0;
    }

    // 相机投影过程
    math::Vec2d projection(math::Vec3d const & p3d){

        math::Vec2d p;

        // 世界坐标系到相机坐标系
        double xc = R_[0] * p3d[0] + R_[1] * p3d[1] + R_[2]* p3d[2] + t_[0];
        double yc = R_[3] * p3d[0] + R_[4] * p3d[1] + R_[5]* p3d[2] + t_[1];
        double zc = R_[6] * p3d[0] + R_[7] * p3d[1] + R_[8]* p3d[2] + t_[2];

        // 相机坐标系到像平面
        double x = xc/zc;
        double y = yc/zc;

        // 径向畸变过程
        double r2 = x*x + y*y;
        double distort_ratio = 1+ dist_[0]* r2+ dist_[1]*r2*r2;

        // 图像坐标系到屏幕坐标系
        p[0] = f_* distort_ratio*x + c_[0];
        p[1] = f_* distort_ratio*y + c_[1];

        return p;

    }

    // 相机在世界坐标中的位置 -R^T*t
    math::Vec3d pos_in_world(){

        math::Vec3d pos;
        pos[0] = R_[0]* t_[0] + R_[3]* t_[1] + R_[6]* t_[2];
        pos[1] = R_[1]* t_[0] + R_[4]* t_[1] + R_[7]* t_[2];
        pos[2] = R_[2]* t_[0] + R_[5]* t_[1] + R_[8]* t_[2];
        return -pos;
    }

    // 相机在世界坐标中的方向
    math::Vec3d dir_in_world(){

        math::Vec3d  dir (R_[6], R_[7],R_[8]);
        return dir;
    }
public:

    // 焦距f
    double f_;

    // 径向畸变系数k1, k2
    double dist_[2];

    // 中心点坐标u0, v0
    double c_[2];

    // 旋转矩阵
    /*
     * [ R_[0], R_[1], R_[2] ]
     * [ R_[3], R_[4], R_[5] ]
     * [ R_[6], R_[7], R_[8] ]
     */
    double R_[9];

    // 平移向量
    double t_[3];
};

int main(int argc, char* argv[]){


    Camera cam;

    //焦距
    cam.f_ = 0.920227;

    // 径向畸变系数
    cam.dist_[0] = -0.106599; cam.dist_[1] = 0.104385;

    // 平移向量
    cam.t_[0] = 0.0814358; cam.t_[1] =  0.937498;   cam.t_[2] = -0.0887441;

    // 旋转矩阵
    cam.R_[0] = 0.999796 ; cam.R_[1] = -0.0127375;  cam.R_[2] =  0.0156807;
    cam.R_[3] = 0.0128557; cam.R_[4] =  0.999894 ;  cam.R_[5] = -0.0073718;
    cam.R_[6] = -0.0155846; cam.R_[7] = 0.00757181; cam.R_[8] = 0.999854;

    // 三维点坐标
    math::Vec3d p3d ={1.36939, -1.17123, 7.04869};

    /*计算相机的投影点*/
    math::Vec2d p2d = cam.projection(p3d);
    std::cout<<"projection coord:\n "<<p2d<<std::endl;
    std::cout<<"result should be:\n 0.208188 -0.035398\n\n";

    /*计算相机在世界坐标系中的位置*/
    math::Vec3d pos = cam.pos_in_world();
    std::cout<<"cam position in world is:\n "<< pos<<std::endl;
    std::cout<<"result should be: \n -0.0948544 -0.935689 0.0943652\n\n";

    /*计算相机在世界坐标系中的方向*/
    math::Vec3d dir = cam.dir_in_world();
    std::cout<<"cam direction in world is:\n "<<dir<<std::endl;
    std::cout<<"result should be: \n -0.0155846 0.00757181 0.999854\n";
}