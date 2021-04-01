#include<cmath>
#include<Eigen/Core>
#include<Eigen/Dense>
#include<iostream>


int main(){
    Eigen::Vector3d p(2.0f, 1.0f, 1.0f);
    Eigen::Matrix3d M1;//��ת����
    Eigen::Matrix3d M2;//ƽ�ƾ���
    float a = 45.0 / 180.0 * acos(-1);
    M1 << cos(a), -1.0 * sin(a), 0,
        sin(a), cos(a), 0,
        0, 0, 1;
    M2 << 1, 0, 1,
        0, 1, 2,
        0, 0, 1;
    p = M2 * M1 * p;
    std::cout << "p��任�������Ϊ " << std::endl;
    std::cout << p<< std::endl;
    return 0;
}