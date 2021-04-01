#include "Triangle.hpp"
#include "rasterizer.hpp"
#include <Eigen/Eigen>
#include <iostream>
#include <opencv2/opencv.hpp>

constexpr double MY_PI = 3.1415926;

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1,
        -eye_pos[2], 0, 0, 0, 1;

    view = translate * view;

    return view;
}

Eigen::Matrix4f get_model_matrix(float rotation_angle)
{
    Eigen::Matrix4f model = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f rotate;  
    float radian = rotation_angle / 180.0 * acos(-1);
    rotate << cos(radian), -sin(radian), 0, 0,
        sin(radian), cos(radian), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;
    model = rotate * model;
    return model;
}

Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar)
{   //aspect_ratio表示近平面的宽高比
    //eye_fov表示视点从近平面最高点到最低点的夹角
    Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f M; //将透视投影转换为正交投影的矩阵
    M<<zNear, 0, 0, 0,
         0, zNear, 0, 0,
         0, 0, zNear+zFar,(-1)*zFar*zNear,
         0, 0, 1, 0;
    float halfEyeAngelRadian = eye_fov/2.0/180.0*MY_PI;
    float t = zNear*std::tan(halfEyeAngelRadian);
    float r=t*aspect_ratio;                     
    float l=(-1)*r;                             
    float b=(-1)*t;                            
    Eigen::Matrix4f M1;
    M1<<2/(r-l),0,0,0,     
        0,2/(t-b),0,0,
        0,0,2/(zNear-zFar),0,
        0,0,0,1;                                //进行拉伸转化为正方形
    Eigen::Matrix4f M2 = Eigen::Matrix4f::Identity();
    M2<<1,0,0,(-1)*(r+l)/2,
        0,1,0,(-1)*(t+b)/2,
        0,0,1,(-1)*(zNear+zFar)/2,
        0,0,0,1;                                // 把正方形的中心转化至原点
    Eigen::Matrix4f M3 = M1 * M2;
    projection = M3 * M;
    return projection;
}

Eigen::Matrix4f get_rotation(Vector3f axis, float angle) {
    Eigen::Matrix4f model = Eigen::Matrix4f::Identity();
    float norm = sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    axis[0] /= norm;
    axis[1] /= norm;
    axis[2] /= norm;
    float radian = angle / 180.0 * acos(-1);
    float x = axis[0], y = axis[1], z = axis[2];
    float s = sin(radian), c = cos(radian);
    model << x*x*(1-c)+c, x*y*(1-c)+z*s, x*z*(1-c)-y*s,
            x*y*(1-c)-z*s, y*y*(1-c)+c, y*z*(1-c)+x*s,
            x*z*(1-c)+y*s, y*z*(1-c)-x*s, z*z*(1-c)+c;
    return model;
}


int main(int argc, const char** argv)
{
    float angle = 0;
    bool command_line = false;
    std::string filename = "output.png";

    if (argc >= 3) {
        command_line = true;
        angle = std::stof(argv[2]); // -r by default
        if (argc == 4) {
            filename = std::string(argv[3]);
        }
        else
            return 0;
    }

    rst::rasterizer r(700, 700);

    Eigen::Vector3f eye_pos = {0, 0, 5};

    std::vector<Eigen::Vector3f> pos{{2, 0, -2}, {0, 2, -2}, {-2, 0, -2}};

    std::vector<Eigen::Vector3i> ind{{0, 1, 2}};

    auto pos_id = r.load_positions(pos);
    auto ind_id = r.load_indices(ind);

    int key = 0;
    int frame_count = 0;

    if (command_line) {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);

        cv::imwrite(filename, image);

        return 0;
    }

    while (key != 27) {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);

        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::imshow("image", image);
        key = cv::waitKey(10);

        std::cout << "frame count: " << frame_count++ << '\n';

        if (key == 'a') {
            angle += 10;
        }
        else if (key == 'd') {
            angle -= 10;
        }
    }

    return 0;
}
