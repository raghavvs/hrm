#include "include/LineIntersection.h"
#include <iostream>

std::vector<Eigen::Vector3d> intersectLineMesh3d(const Eigen::VectorXd& line,
                                                 const MeshMatrix& shape) {
    std::vector<Eigen::Vector3d> points;
    Eigen::Vector3d t0, u, v, pt;

    for (auto i = 0; i < shape.faces.rows(); ++i) {
        // find triangle edge vectors
        t0 = shape.vertices.col(int(shape.faces(i, 0)));
        u = shape.vertices.col(int(shape.faces(i, 1))) - t0;
        v = shape.vertices.col(int(shape.faces(i, 2))) - t0;

        // keep only interesting points
        bool hasIntersect = intersectLineTriangle3d(&line, &t0, &u, &v, &pt);

        if (hasIntersect) {
            points.push_back(pt);
        }

        if (points.size() == 2) {
            break;
        }
    }

    return points;
}

std::vector<Eigen::Vector3d> intersectVerticalLineMesh3d(
    const Eigen::VectorXd& line, const MeshMatrix& shape) {
    std::vector<Eigen::Vector3d> points;

    if (line(0) > shape.vertices.row(0).maxCoeff() ||
        line(0) < shape.vertices.row(0).minCoeff()) {
        return points;
    }
    if (line(1) > shape.vertices.row(1).maxCoeff() ||
        line(1) < shape.vertices.row(1).minCoeff()) {
        return points;
    }

    Eigen::Vector3d t0, u, v, pt;

    /*
     * \brief Specifially for vertical sweep lines, first do a quick check
     * according to x and y coord: If the x or y coordinates of the vertical
     * sweep line is out of range of the triangle, directly ignore
     */
    for (auto i = 0; i < shape.faces.rows(); ++i) {
        // ignore the face that is out of range
        if (line(0) <
                std::fmin(
                    shape.vertices(0, int(shape.faces(i, 0))),
                    std::fmin(shape.vertices(0, int(shape.faces(i, 1))),
                              shape.vertices(0, int(shape.faces(i, 2))))) ||
            line(0) >
                std::fmax(
                    shape.vertices(0, int(shape.faces(i, 0))),
                    std::fmax(shape.vertices(0, int(shape.faces(i, 1))),
                              shape.vertices(0, int(shape.faces(i, 2)))))) {
            continue;
        }
        if (line(1) <
                std::fmin(
                    shape.vertices(1, int(shape.faces(i, 0))),
                    std::fmin(shape.vertices(1, int(shape.faces(i, 1))),
                              shape.vertices(1, int(shape.faces(i, 2))))) ||
            line(1) >
                std::fmax(
                    shape.vertices(1, int(shape.faces(i, 0))),
                    std::fmax(shape.vertices(1, int(shape.faces(i, 1))),
                              shape.vertices(1, int(shape.faces(i, 2)))))) {
            continue;
        }

        // find triangle edge vectors
        t0 = shape.vertices.col(int(shape.faces(i, 0)));
        u = shape.vertices.col(int(shape.faces(i, 1))) - t0;
        v = shape.vertices.col(int(shape.faces(i, 2))) - t0;

        // keep only interesting points
        bool hasIntersect = intersectLineTriangle3d(&line, &t0, &u, &v, &pt);

        if (hasIntersect) {
            points.push_back(pt);
        }

        if (points.size() == 2) {
            break;
        }
    }

    return points;
}

bool intersectLineTriangle3d(const Eigen::VectorXd* line,
                             const Eigen::Vector3d* t0,
                             const Eigen::Vector3d* u, const Eigen::Vector3d* v,
                             Eigen::Vector3d* pt) {
    double tol = 1e-12;
    Eigen::Vector3d n;
    double a, b, uu, uv, vv, wu, wv, D, s, t;

    // triangle normal
    n = u->cross(*v);
    n.normalize();

    // vector between triangle origin and line origin
    a = -n.dot(line->head(3) - *t0);
    b = n.dot(line->tail(3));
    if (!((std::fabs(b) > tol) && (n.norm() > tol))) {
        return false;
    }

    /* \brief Compute intersection point of line with supporting plane:
                  If pos = a/b < 0: point before ray
                  IF pos = a/b > |dir|: point after edge*/
    // coordinates of intersection point
    *pt = line->head(3) + a / b * line->tail(3);

    // Test if intersection point is inside triangle
    // normalize direction vectors of triangle edges
    uu = u->dot(*u);
    uv = u->dot(*v);
    vv = v->dot(*v);

    // coordinates of vector v in triangle basis
    wu = u->dot(*pt - *t0);
    wv = v->dot(*pt - *t0);

    // normalization constant
    D = pow(uv, 2) - uu * vv;

    // test first coordinate
    s = (uv * wv - vv * wu) / D;
    if ((s < -tol) || (s > 1.0 + tol)) {
        return false;
    }

    // test second coordinate and third triangle edge
    t = (uv * wu - uu * wv) / D;
    if ((t < -tol) || (s + t > 1.0 + tol)) {
        return false;
    }

    return true;
}

std::vector<double> intersectHorizontalLinePolygon2d(
    const double ty, const Eigen::MatrixXd& shape) {
    std::vector<double> points;

    // ignore the edge that is out of range
    if (ty > shape.row(1).maxCoeff() || ty < shape.row(1).minCoeff()) {
        return points;
    }

    for (auto i = 0; i < shape.cols(); ++i) {
        double x1 = shape(0, i);
        double y1 = shape(1, i);
        double x2;
        double y2;

        if (i == shape.cols() - 1) {
            x2 = shape(0, 0);
            y2 = shape(1, 0);
        } else {
            x2 = shape(0, i + 1);
            y2 = shape(1, i + 1);
        }

        // compute line-line intersection
        if (ty >= std::fmin(y1, y2) && ty <= std::fmax(y1, y2)) {
            double t = (ty - y2) / (y1 - y2);
            if (t >= 0.0 && t <= 1.0) {
                points.push_back(t * x1 + (1.0 - t) * x2);
            }
        }
    }

    return points;
}
