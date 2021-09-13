#include "include/ParsePlanningSettings.h"

void loadVectorGeometry(const std::string config_file,
                        const int num_curve_param,
                        std::vector<SuperEllipse>& object) {
    std::vector<std::vector<double>> object_config =
        parse2DCsvFile(config_file);

    // Arena and Obstacles as class of SuperEllipse
    object.clear();
    for (size_t j = 0; j < object_config.size(); j++) {
        object.emplace_back(SuperEllipse(
            {object_config[j][0], object_config[j][1]}, object_config[j][2],
            {object_config[j][3], object_config[j][4]}, object_config[j][5],
            num_curve_param));
    }
}

void loadVectorGeometry(const std::string config_file, const int num_surf_param,
                        std::vector<SuperQuadrics>& object) {
    // Read config file
    std::vector<std::vector<double>> object_config =
        parse2DCsvFile(config_file);

    // Generate SQ object (orientation from Quaternion parameterization)
    object.clear();
    for (size_t j = 0; j < object_config.size(); j++) {
        object.emplace_back(SuperQuadrics(
            {object_config[j][0], object_config[j][1], object_config[j][2]},
            {object_config[j][3], object_config[j][4]},
            {object_config[j][5], object_config[j][6], object_config[j][7]},
            Eigen::Quaterniond(object_config[j][8], object_config[j][9],
                               object_config[j][10], object_config[j][11]),
            num_surf_param));
    }
}

MultiBodyTree2D loadRobotMultiBody2D(const std::string path_prefix,
                                     const int num_curve_param) {
    // Read robot config file
    std::vector<SuperEllipse> robot_parts;
    loadVectorGeometry(path_prefix + "robot_config_2D.csv", num_curve_param,
                       robot_parts);

    // Generate multibody tree for robot
    MultiBodyTree2D robot(robot_parts[0]);
    for (size_t i = 1; i < robot_parts.size(); ++i) {
        robot.addBody(robot_parts[i]);
    }

    return robot;
}

MultiBodyTree3D loadRobotMultiBody3D(const std::string path_prefix,
                                     const std::string quat_file,
                                     const int num_surf_param) {
    // Read and setup robot info
    std::vector<SuperQuadrics> robot_parts;
    loadVectorGeometry(path_prefix + "robot_config_3D.csv", num_surf_param,
                       robot_parts);

    loadPreDefinedQuaternions(quat_file, robot_parts[0]);

    // Generate multibody tree for robot
    MultiBodyTree3D robot(robot_parts[0]);
    for (size_t i = 1; i < robot_parts.size(); i++) {
        robot.addBody(robot_parts[i]);
    }

    return robot;
}

void loadPreDefinedQuaternions(const std::string quat_file,
                               SuperQuadrics& robot_base) {
    // Read predefined quaternions
    if (quat_file.compare("0") == 0) {
        std::cout << "Will generate uniform random rotations from SO(3)"
                  << std::endl;
    } else {
        std::vector<std::vector<double>> quat_sample =
            parse2DCsvFile(quat_file);

        std::vector<Eigen::Quaterniond> q_sample;
        for (size_t i = 0; i < quat_sample.size(); i++) {
            Eigen::Quaterniond q;
            q.w() = quat_sample[i][3];
            q.x() = quat_sample[i][0];
            q.y() = quat_sample[i][1];
            q.z() = quat_sample[i][2];
            q_sample.emplace_back(q);
        }
        robot_base.setQuatSamples(q_sample);
    }
}
