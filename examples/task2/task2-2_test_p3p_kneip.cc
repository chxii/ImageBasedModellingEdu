// Created by sway on 2018/8/25.
/*
 * POSIT (Pose from Orthography and Scaling with iterations), 比例正交
 * 投影迭代变换算法，适用条件是物体在Z轴方向的厚度远小于其在Z轴方向的平均深度。
 */
#include <complex>
#include <algorithm>
#include<set>
#include <iostream>

#include "math/matrix_tools.h"
#include "math/matrix.h"
#include "math/vector.h"
#include "sfm/correspondence.h"
#include "sfm/defines.h"
#include "util/system.h"

typedef math::Matrix<double, 3, 4> Pose;
typedef std::vector<Pose> PutativePoses;

/**
 *
 * @param factors
 * @param real_roots
 */
void solve_quartic_roots (math::Vec5d const& factors, math::Vec4d* real_roots)
{
    double const A = factors[0];
    double const B = factors[1];
    double const C = factors[2];
    double const D = factors[3];
    double const E = factors[4];

    double const A2 = A * A;
    double const B2 = B * B;
    double const A3 = A2 * A;
    double const B3 = B2 * B;
    double const A4 = A3 * A;
    double const B4 = B3 * B;

    double const alpha = -3.0 * B2 / (8.0 * A2) + C / A;
    double const beta = B3 / (8.0 * A3)- B * C / (2.0 * A2) + D / A;
    double const gamma = -3.0 * B4 / (256.0 * A4) + B2 * C / (16.0 * A3) - B * D / (4.0 * A2) + E / A;

    double const alpha2 = alpha * alpha;
    double const alpha3 = alpha2 * alpha;
    double const beta2 = beta * beta;

    std::complex<double> P(-alpha2 / 12.0 - gamma, 0.0);
    std::complex<double> Q(-alpha3 / 108.0 + alpha * gamma / 3.0 - beta2 / 8.0, 0.0);
    std::complex<double> R = -Q / 2.0 + std::sqrt(Q * Q / 4.0 + P * P * P / 27.0);

    std::complex<double> U = std::pow(R, 1.0 / 3.0);
    std::complex<double> y = (U.real() == 0.0)
                             ? -5.0 * alpha / 6.0 - std::pow(Q, 1.0 / 3.0)
                             : -5.0 * alpha / 6.0 - P / (3.0 * U) + U;

    std::complex<double> w = std::sqrt(alpha + 2.0 * y);
    std::complex<double> part1 = -B / (4.0 * A);
    std::complex<double> part2 = 3.0 * alpha + 2.0 * y;
    std::complex<double> part3 = 2.0 * beta / w;

    std::complex<double> complex_roots[4];
    complex_roots[0] = part1 + 0.5 * (w + std::sqrt(-(part2 + part3)));
    complex_roots[1] = part1 + 0.5 * (w - std::sqrt(-(part2 + part3)));
    complex_roots[2] = part1 + 0.5 * (-w + std::sqrt(-(part2 - part3)));
    complex_roots[3] = part1 + 0.5 * (-w - std::sqrt(-(part2 - part3)));

    for (int i = 0; i < 4; ++i)
        (*real_roots)[i] = complex_roots[i].real();
}


/**
 *
 * @param p1
 * @param p2
 * @param p3
 * @param f1
 * @param f2
 * @param f3
 * @param solutions
 */
void pose_p3p_kneip (
        math::Vec3d p1, math::Vec3d p2, math::Vec3d p3,
        math::Vec3d f1, math::Vec3d f2, math::Vec3d f3,
        std::vector<math::Matrix<double, 3, 4> >* solutions){

    /* Check if points are co-linear. In this case return no solution. */
    double const colinear_threshold = 1e-10;
    if ((p2 - p1).cross(p3 - p1).square_norm() < colinear_threshold){
        solutions->clear();
        return;
    }

    /* Normalize directions if necessary. */
    double const normalize_epsilon = 1e-10;
    if (!MATH_EPSILON_EQ(f1.square_norm(), 1.0, normalize_epsilon))
        f1.normalize();
    if (!MATH_EPSILON_EQ(f2.square_norm(), 1.0, normalize_epsilon))
        f2.normalize();
    if (!MATH_EPSILON_EQ(f3.square_norm(), 1.0, normalize_epsilon))
        f3.normalize();

    /* Create camera frame. */
    math::Matrix3d T;
    {
        math::Vec3d e1 = f1;
        math::Vec3d e3 = f1.cross(f2).normalized();
        math::Vec3d e2 = e3.cross(e1);
        std::copy(e1.begin(), e1.end(), T.begin() + 0);
        std::copy(e2.begin(), e2.end(), T.begin() + 3);
        std::copy(e3.begin(), e3.end(), T.begin() + 6);
        f3 = T * f3;
    }

    /* Change camera frame and point order if f3[2] > 0. */
    if (f3[2] > 0.0)
    {
        std::swap(p1, p2);
        std::swap(f1, f2);

        math::Vec3d e1 = f1;
        math::Vec3d e3 = f1.cross(f2).normalized();
        math::Vec3d e2 = e3.cross(e1);
        std::copy(e1.begin(), e1.end(), T.begin() + 0);
        std::copy(e2.begin(), e2.end(), T.begin() + 3);
        std::copy(e3.begin(), e3.end(), T.begin() + 6);
        f3 = T * f3;
    }

    /* Create world frame. */
    math::Matrix3d N;
    {
        math::Vec3d n1 = (p2 - p1).normalized();
        math::Vec3d n3 = n1.cross(p3 - p1).normalized();
        math::Vec3d n2 = n3.cross(n1);
        std::copy(n1.begin(), n1.end(), N.begin() + 0);
        std::copy(n2.begin(), n2.end(), N.begin() + 3);
        std::copy(n3.begin(), n3.end(), N.begin() + 6);
    }
    p3 = N * (p3 - p1);

    /* Extraction of known parameters. */
    double d_12 = (p2 - p1).norm();
    double f_1 = f3[0] / f3[2];
    double f_2 = f3[1] / f3[2];
    double p_1 = p3[0];
    double p_2 = p3[1];

    double cos_beta = f1.dot(f2);
    double b = 1.0 / (1.0 - MATH_POW2(cos_beta)) - 1;

    if (cos_beta < 0.0)
        b = -std::sqrt(b);
    else
        b = std::sqrt(b);

    /* Temporary pre-computed variables. */
    double f_1_pw2 = MATH_POW2(f_1);
    double f_2_pw2 = MATH_POW2(f_2);
    double p_1_pw2 = MATH_POW2(p_1);
    double p_1_pw3 = p_1_pw2 * p_1;
    double p_1_pw4 = p_1_pw3 * p_1;
    double p_2_pw2 = MATH_POW2(p_2);
    double p_2_pw3 = p_2_pw2 * p_2;
    double p_2_pw4 = p_2_pw3 * p_2;
    double d_12_pw2 = MATH_POW2(d_12);
    double b_pw2 = MATH_POW2(b);

    /* Factors of the 4th degree polynomial. */
    math::Vec5d factors;
    factors[0] = - f_2_pw2 * p_2_pw4 - p_2_pw4 * f_1_pw2 - p_2_pw4;

    factors[1] = 2.0 * p_2_pw3 * d_12 * b
                 + 2.0 * f_2_pw2 * p_2_pw3 * d_12 * b
                 - 2.0 * f_2 * p_2_pw3 * f_1 * d_12;

    factors[2] = - f_2_pw2 * p_2_pw2 * p_1_pw2
                 - f_2_pw2 * p_2_pw2 * d_12_pw2 * b_pw2
                 - f_2_pw2 * p_2_pw2 * d_12_pw2
                 + f_2_pw2 * p_2_pw4
                 + p_2_pw4 * f_1_pw2
                 + 2.0 * p_1 * p_2_pw2 * d_12
                 + 2.0 * f_1 * f_2 * p_1 * p_2_pw2 * d_12 * b
                 - p_2_pw2 * p_1_pw2 * f_1_pw2
                 + 2.0 * p_1 * p_2_pw2 * f_2_pw2 * d_12
                 - p_2_pw2 * d_12_pw2 * b_pw2
                 - 2.0 * p_1_pw2 * p_2_pw2;

    factors[3] = 2.0 * p_1_pw2 * p_2 * d_12 * b
                 + 2.0 * f_2 * p_2_pw3 * f_1 * d_12
                 - 2.0 * f_2_pw2 * p_2_pw3 * d_12 * b
                 - 2.0 * p_1 * p_2 * d_12_pw2 * b;

    factors[4] = -2.0 * f_2 * p_2_pw2 * f_1 * p_1 * d_12 * b
                 + f_2_pw2 * p_2_pw2 * d_12_pw2
                 + 2.0 * p_1_pw3 * d_12
                 - p_1_pw2 * d_12_pw2
                 + f_2_pw2 * p_2_pw2 * p_1_pw2
                 - p_1_pw4
                 - 2.0 * f_2_pw2 * p_2_pw2 * p_1 * d_12
                 + p_2_pw2 * f_1_pw2 * p_1_pw2
                 + f_2_pw2 * p_2_pw2 * d_12_pw2 * b_pw2;

    /* Solve for the roots of the polynomial. */
    math::Vec4d real_roots;
    solve_quartic_roots(factors, &real_roots);

    /* Back-substitution of each solution. */
    solutions->clear();
    solutions->resize(4);
    for (int i = 0; i < 4; ++i)
    {
        double cot_alpha = (-f_1 * p_1 / f_2 - real_roots[i] * p_2 + d_12 * b)
                           / (-f_1 * real_roots[i] * p_2 / f_2 + p_1 - d_12);

        double cos_theta = real_roots[i];
        double sin_theta = std::sqrt(1.0 - MATH_POW2(real_roots[i]));
        double sin_alpha = std::sqrt(1.0 / (MATH_POW2(cot_alpha) + 1));
        double cos_alpha = std::sqrt(1.0 - MATH_POW2(sin_alpha));

        if (cot_alpha < 0.0)
            cos_alpha = -cos_alpha;

        math::Vec3d C(
                d_12 * cos_alpha * (sin_alpha * b + cos_alpha),
                cos_theta * d_12 * sin_alpha * (sin_alpha * b + cos_alpha),
                sin_theta * d_12 * sin_alpha * (sin_alpha * b + cos_alpha));

        C = p1 + N.transposed() * C;

        math::Matrix3d R;
        R[0] = -cos_alpha; R[1] = -sin_alpha * cos_theta; R[2] = -sin_alpha * sin_theta;
        R[3] = sin_alpha;  R[4] = -cos_alpha * cos_theta; R[5] = -cos_alpha * sin_theta;
        R[6] = 0.0;        R[7] = -sin_theta;             R[8] = cos_theta;

        R = N.transposed().mult(R.transposed()).mult(T);

        /* Convert camera position and cam-to-world rotation to pose. */
        R = R.transposed();
        C = -R * C;

        solutions->at(i) = R.hstack(C);
    }
}

/**
 *
 * @param corresp
 * @param inv_k_matrix
 * @param poses
 */
void compute_p3p (sfm::Correspondences2D3D const& corresp,
                            math::Matrix<double, 3, 3> const& inv_k_matrix,
                  PutativePoses* poses){

    if (corresp.size() < 3)
        throw std::invalid_argument("At least 3 correspondences required");

    /* Draw 3 unique random numbers. */
    std::set<int> result;
    while (result.size() < 3)
        result.insert(util::system::rand_int() % corresp.size());

    std::set<int>::const_iterator iter = result.begin();
    sfm::Correspondence2D3D const& c1(corresp[*iter++]);
    sfm::Correspondence2D3D const& c2(corresp[*iter++]);
    sfm::Correspondence2D3D const& c3(corresp[*iter]);

    //
    pose_p3p_kneip(
            math::Vec3d(c1.p3d), math::Vec3d(c2.p3d), math::Vec3d(c3.p3d),
            inv_k_matrix.mult(math::Vec3d(c1.p2d[0], c1.p2d[1], 1.0)),
            inv_k_matrix.mult(math::Vec3d(c2.p2d[0], c2.p2d[1], 1.0)),
            inv_k_matrix.mult(math::Vec3d(c3.p2d[0], c3.p2d[1], 1.0)),
            poses);
}


int main(int argc, char*argv[]){

    // intrinsic matrix
    math::Matrix<double, 3, 3>k_matrix;
    k_matrix.fill(0.0);
    k_matrix[0] = 0.972222;
    k_matrix[2] = 0.0; // cx =0 图像上的点已经进行了归一化（图像中心为原点，图像尺寸较长的边归一化为1）
    k_matrix[4] = 0.972222;
    k_matrix[5] = 0.0; // cy=0  图像上的点已经进行了归一化（图像中心为原点，图像尺寸较长的边归一化为1）
    k_matrix[8] = 1.0;

    math::Matrix<double, 3, 3> inv_k_matrix = math::matrix_inverse(k_matrix);
    std::cout<<"inverse k matrix: "<<inv_k_matrix<<std::endl;
//    inverse k matrix: 1.02857 0 0
//    0 1.02857 0
//    0 0 1

    // 世界坐标系汇总3D点的坐标
    math::Vec3d p1(-2.57094,-0.217018, 6.05338);
    math::Vec3d p2(-0.803123, 0.251818, 6.98383);
    math::Vec3d p3(2.05584, -0.607918, 7.52573);

    // 对应的图像坐标系中的坐标（图像中心为原点，以图像长或宽归一化到[-0.5,0.5]之间。
    math::Vec2d uv1(-0.441758,-0.185523);
    math::Vec2d uv2(-0.135753,-0.0920593);
    math::Vec2d uv3(0.243795,-0.192743);

    // 计算相机坐标系中对应的摄线
    math::Vec3d f1 = inv_k_matrix.mult(math::Vec3d(uv1[0], uv1[1], 1.0));
    math::Vec3d f2 = inv_k_matrix.mult(math::Vec3d(uv2[0], uv2[1], 1.0));
    math::Vec3d f3 = inv_k_matrix.mult(math::Vec3d(uv3[0], uv3[1], 1.0));

//    math::Vec3d f1(-0.454379, -0.190824, 1);
//    math::Vec3d f2(-0.139631, -0.0946896, 1);
//    math::Vec3d f3(0.25076, -0.19825, 1);

   // kneip p3p计算出4组解
    std::vector<math::Matrix<double, 3, 4> >solutions;
    pose_p3p_kneip (p1, p2, p3, f1, f2, f3, &solutions);
    for(int i=0; i<solutions.size(); i++){
        std::cout<<"solution "<<i<<": "<<std::endl<<solutions[i]<<std::endl;
    }

    std::cout<<"the result should be \n"
    << "solution 0:"<<std::endl;
    std::cout<< "0.255193 -0.870436 -0.420972 3.11342\n"
    << "0.205372 0.474257 -0.856097 5.85432\n"
    << "0.944825 0.132022 0.299794 0.427496\n\n";

    std::cout<<"solution 1:"<<std::endl;
    std::cout<<"0.255203 -0.870431 -0.420976 3.11345\n"
    <<"0.205372 0.474257 -0.856097 5.85432\n"
    <<"0.944825 0.132022 0.299794 0.427496\n\n";

    std::cout<<"solution 2:"<<std::endl;
    std::cout<<"0.999829 -0.00839209 -0.0164611 -0.0488599\n"
    <<"0.00840016 0.999965 0.000421432 -0.905071\n"
    <<"0.016457 -0.000559636 0.999864 -0.0303736\n\n";

    std::cout<<"solution 3:"<<std::endl;
    std::cout<<"0.975996 0.122885 0.179806 -1.4207\n"
    <<"-0.213274 0.706483 0.67483 -5.68453\n"
    <<"-0.0441038 -0.69698 0.715733 1.71501\n\n";


    // 通过第4个点的投影计算正确的姿态
    math::Vec3d p4(-0.62611418962478638, -0.80525958538055419, 6.7783102989196777);
    math::Vec2d uv4(-0.11282696574926376,-0.24667978286743164);
    //const double thresh = 0.005;

    /* Check all putative solutions and count inliers. */
    for (std::size_t j = 0; j < solutions.size(); ++j){
        math::Vec4d p3d(p4[0], p4[1], p4[2], 1.0);
        math::Vec3d p2d = k_matrix * (solutions[j] * p3d);
        double square_error = MATH_POW2(p2d[0] / p2d[2] - uv4[0])
                              + MATH_POW2(p2d[1] / p2d[2] - uv4[1]);
        std::cout<<"reproj err of solution "<<j<<" "<<square_error<<std::endl;
    }

    return 0;
}
