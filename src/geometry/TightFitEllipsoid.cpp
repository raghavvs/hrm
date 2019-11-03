#include "include/TightFitEllipsoid.h"
#include "util/include/InterpolateSE3.h"

SuperEllipse getMVCE2D(const std::vector<double>& a,
                       const std::vector<double>& b, const double thetaA,
                       const double thetaB, const unsigned int num) {
    Eigen::Matrix2d Ra = Eigen::Rotation2Dd(thetaA).matrix(),
                    Rb = Eigen::Rotation2Dd(thetaB).matrix();

    double r = fmin(b[0], b[1]);
    Eigen::DiagonalMatrix<double, 2> diag, diag_a, diag_c;
    diag.diagonal() = Eigen::Array2d(r / b[0], r / b[1]);
    diag_a.diagonal() = Eigen::Array2d(pow(a[0], -2), pow(a[1], -2));

    // Shrinking affine transformation
    Eigen::Matrix2d T = Rb * diag * Rb.transpose();

    // In shrunk space, fit ellipsoid Cp to sphere Bp and ellipsoid Ap
    Eigen::Matrix2d Ap =
        T.inverse() * (Ra * diag_a * Ra.transpose()) * T.inverse();
    Eigen::JacobiSVD<Eigen::Matrix2d> svd(
        Ap, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Array2d a_p = svd.singularValues().array().pow(-0.5);
    Eigen::Array2d c_p = {std::max(a_p(0), r), std::max(a_p(1), r)};

    // Stretch back
    diag_c.diagonal() = c_p.pow(-2);
    Eigen::Matrix2d C =
        T * svd.matrixU() * diag_c * svd.matrixU().transpose() * T;
    svd.compute(C);
    double ang_c = acos(svd.matrixU()(0, 0));
    Eigen::Array2d c = svd.singularValues().array().pow(-0.5);

    return SuperEllipse({c(0), c(1)}, 1, {0, 0}, ang_c, num);
}

SuperQuadrics getMVCE3D(const std::vector<double>& a,
                        const std::vector<double>& b,
                        const Eigen::Quaterniond& quatA,
                        const Eigen::Quaterniond& quatB,
                        const unsigned int num) {
    Eigen::Matrix3d Ra = quatA.toRotationMatrix();
    Eigen::Matrix3d Rb = quatB.toRotationMatrix();

    double r = fmin(b[0], fmin(b[1], b[2]));
    Eigen::DiagonalMatrix<double, 3> diag, diag_a, diag_c;
    diag.diagonal() = Eigen::Array3d(r / b[0], r / b[1], r / b[2]);
    diag_a.diagonal() =
        Eigen::Array3d(pow(a[0], -2), pow(a[1], -2), pow(a[2], -2));

    // Shrinking affine transformation
    Eigen::Matrix3d T = Rb * diag * Rb.transpose();

    // In shrunk space, fit ellipsoid Cp to sphere Bp and ellipsoid Ap
    Eigen::Matrix3d Ap =
        T.inverse() * (Ra * diag_a * Ra.transpose()) * T.inverse();
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(
        Ap, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Array3d a_p = svd.singularValues().array().pow(-0.5);
    Eigen::Array3d c_p = {std::fmax(a_p(0), r), std::fmax(a_p(1), r),
                          std::fmax(a_p(2), r)};

    // Stretch back
    diag_c.diagonal() = c_p.pow(-2);
    Eigen::Matrix3d C =
        T * svd.matrixU() * diag_c * svd.matrixU().transpose() * T;
    svd.compute(C);
    Eigen::Quaterniond q_c(svd.matrixU());
    Eigen::Array3d c = svd.singularValues().array().pow(-0.5);

    return SuperQuadrics({c(0), c(1), c(2)}, {1, 1}, {0, 0, 0},
                         {q_c.w(), q_c.x(), q_c.y(), q_c.z()}, num);
}

SuperQuadrics getTFE3D(const std::vector<double>& a,
                       const Eigen::Quaterniond& quatA,
                       const Eigen::Quaterniond& quatB,
                       const unsigned int N_step, const unsigned int num) {
    std::vector<Eigen::Quaterniond> interpolatedQuat =
        interpolateAngleAxis(quatA, quatB, N_step);

    // Iteratively compute MVCE and update
    SuperQuadrics enclosedEllipsoid = getMVCE3D(a, a, quatA, quatB, num);
    for (size_t i = 1; i < size_t(N_step); ++i) {
        enclosedEllipsoid = getMVCE3D(a, enclosedEllipsoid.getSemiAxis(),
                                      interpolatedQuat.at(i),
                                      enclosedEllipsoid.getQuaternion(), num);
    }

    return enclosedEllipsoid;
}
