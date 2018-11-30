#ifndef SUPERELLIPSE_H
#define SUPERELLIPSE_H

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>
#include <unsupported/Eigen/Polynomials>

using namespace Eigen;

class SuperEllipse{
public:
    /* Parameters of superellipse
          a : semi-axes length;
        ang : rotational angle;
        eps : epsilon;
        pos : position of the center.
    */
    struct shape{
        double a[2];
        double ang;
        double eps;
        double pos[2];
    } Shape;

    // Number of points on boundary
    int num;

    // Functions
    //SuperEllipse(double a[6], int num);
    MatrixXd originShape();
    MatrixXd minkSum2D(shape Shape_b, int K);
    double expFun(double th, double p, bool func);
};

#endif // SUPERELLIPSE_H
