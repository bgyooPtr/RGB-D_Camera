#include "camera_matrix.h"

_intrinsics_::_intrinsics_()
	:width(0), height(0), ppx(0.f), ppy(0.f), fx(0.f), fy(0.f)
{
	memset(coeffs, 0, sizeof(float) * 5);
}

_intrinsics_::_intrinsics_(const _intrinsics_& intr) {
	width = intr.width; height = intr.height; ppx = intr.ppx; ppy = intr.ppy; fx = intr.fx; fy = intr.fy;
	memcpy(coeffs, intr.coeffs, sizeof(float) * 5);
}

_extrinsics_::_extrinsics_()
{
	rotation[0] = rotation[4] = rotation[8] = 1.f;
	rotation[1] = rotation[2] = rotation[3] = rotation[5] = rotation[6] = rotation[7]
		= translation[0] = translation[1] = translation[2] = 0.f;
}

_extrinsics_::_extrinsics_(const _extrinsics_& ext)
{
	memcpy(rotation, ext.rotation, sizeof(float) * 9);
	memcpy(translation, ext.translation, sizeof(float) * 3);
}
bool _rgbd_extrinsics_::isIdentity() const
{
	return (rotation[0] == 1) && (rotation[4] == 1) && (translation[0] == 0) && (translation[1] == 0) && (translation[2] == 0);
}
_rgbd_parameters_::_rgbd_parameters_()
{
}

_rgbd_parameters_::_rgbd_parameters_(const _rgbd_parameters_& rparam)
{
	this->cam_id = rparam.cam_id;
	this->color_intrinsic = rparam.color_intrinsic;
	this->depth_intrinsic = rparam.depth_intrinsic;
	this->depth_to_color = rparam.depth_to_color;
}

_camera_raw_data_::_camera_raw_data_()
{

}

_camera_raw_data_::_camera_raw_data_(const _camera_raw_data_& crd)
{
	this->colorData = crd.colorData;
	this->depthData = crd.depthData;
	this->colorTime = crd.colorTime;
	this->depthTime = crd.depthTime;
	this->rgbdParam = crd.rgbdParam;
}

void project_point_to_pixel(
	float pixel[2],
	const struct _intrinsics_ * intrin,
	const float point[3],
	bool coeffs
)
{
	float x = point[0] / point[2], y = point[1] / point[2];

	if (coeffs)
	{
		float r2 = x*x + y*y;
		float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2*r2 + intrin->coeffs[4] * r2*r2*r2;
		x *= f;
		y *= f;
		float dx = x + 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
		float dy = y + 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
		x = dx;
		y = dy;
	}

	pixel[0] = x * intrin->fx + intrin->ppx;
	pixel[1] = y * intrin->fy + intrin->ppy;
}

void deproject_pixel_to_point(
	float point[3],
	const struct _intrinsics_* intrin,
	const float pixel[2],
	float depth,
	bool coeffs
)
{
	float x = (pixel[0] - intrin->ppx) / intrin->fx;
	float y = (pixel[1] - intrin->ppy) / intrin->fy;

	if (coeffs)
	{
		float r2 = x*x + y*y;
		float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2*r2 + intrin->coeffs[4] * r2*r2*r2;
		float ux = x*f + 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
		float uy = y*f + 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
		x = ux;
		y = uy;
	}

	point[0] = depth * x;
	point[1] = depth * y;
	point[2] = depth;
}

void transform_point_to_point(
	float to_point[3],
	const struct _extrinsics_ * extrin,
	const float from_point[3])
{
	to_point[0] = extrin->rotation[0] * from_point[0] + extrin->rotation[1] * from_point[1] + extrin->rotation[2] * from_point[2] + extrin->translation[0];
	to_point[1] = extrin->rotation[3] * from_point[0] + extrin->rotation[4] * from_point[1] + extrin->rotation[5] * from_point[2] + extrin->translation[1];
	to_point[2] = extrin->rotation[6] * from_point[0] + extrin->rotation[7] * from_point[1] + extrin->rotation[8] * from_point[2] + extrin->translation[2];
}

void transform_point_to_point(float to_point[3], float rotation[9], float translation[3], const float from_point[3])
{
	to_point[0] = rotation[0] * from_point[0] + rotation[3] * from_point[1] + rotation[6] * from_point[2] + translation[0];
	to_point[1] = rotation[1] * from_point[0] + rotation[4] * from_point[1] + rotation[7] * from_point[2] + translation[1];
	to_point[2] = rotation[2] * from_point[0] + rotation[5] * from_point[1] + rotation[8] * from_point[2] + translation[2];
};

void calculate_depth_to_color_matrix(const struct _rgbd_parameters_* data, float* transform_matrix)
{
	Eigen::Matrix<double, 4, 4> depthK;
	Eigen::Matrix<double, 4, 4> depthK_inv;
	Eigen::Matrix<double, 4, 4> colorK;
	Eigen::Matrix<double, 4, 4> d2c;
	Eigen::Matrix<double, 4, 4> dst;

	depthK << data->depth_intrinsic.fx, 0.f, data->depth_intrinsic.ppx, 0.f,
		0.f, data->depth_intrinsic.fy, data->depth_intrinsic.ppy, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f;
	colorK << data->color_intrinsic.fx, 0.f, data->color_intrinsic.ppx, 0.f,
		0.f, data->color_intrinsic.fy, data->color_intrinsic.ppy, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f;
	d2c << data->depth_to_color.rotation[0], data->depth_to_color.rotation[1], data->depth_to_color.rotation[2], data->depth_to_color.translation[0],
		data->depth_to_color.rotation[3], data->depth_to_color.rotation[4], data->depth_to_color.rotation[5], data->depth_to_color.translation[1],
		data->depth_to_color.rotation[6], data->depth_to_color.rotation[7], data->depth_to_color.rotation[8], data->depth_to_color.translation[2],
		0.f, 0.f, 0.f, 1.f;
	dst = colorK * d2c * depthK.inverse();

	transform_matrix[0] = dst(0, 0);
	transform_matrix[1] = dst(0, 1);
	transform_matrix[2] = dst(0, 2);
	transform_matrix[3] = dst(0, 3);
	transform_matrix[4] = dst(1, 0);
	transform_matrix[5] = dst(1, 1);
	transform_matrix[6] = dst(1, 2);
	transform_matrix[7] = dst(1, 3);
	transform_matrix[8] = dst(2, 0);
	transform_matrix[9] = dst(2, 1);
	transform_matrix[10] = dst(2, 2);
	transform_matrix[11] = dst(2, 3);
	transform_matrix[12] = dst(3, 0);
	transform_matrix[13] = dst(3, 1);
	transform_matrix[14] = dst(3, 2);
	transform_matrix[15] = dst(3, 3);

	std::cout << data->cam_id << std::endl;
	std::cout << "depth_K_Mat" << std::endl << depthK << std::endl;
	std::cout << "color_K_Mat" << std::endl << colorK << std::endl;
	std::cout << "depth2color_Mat" << std::endl << d2c << std::endl;
	std::cout << "All_Mat" << std::endl << dst << std::endl << std::endl;
}

void readParameterYaml(std::string full_path, struct _rgbd_parameters_ * data)
{
	cv::FileStorage fs(full_path, cv::FileStorage::READ);
	if (!fs.isOpened()) {
		throw std::runtime_error("file is not opened: " + full_path);
		return;
	}

	fs["cam_name"] >> data->cam_id;
	cv::Mat k, coeffs, R, t;
	fs["rgb_intrinsic"] >> k;
	data->color_intrinsic.fx = k.at<float>(0, 0); data->color_intrinsic.ppx = k.at<float>(0, 2);
	data->color_intrinsic.fy = k.at<float>(1, 1); data->color_intrinsic.ppy = k.at<float>(1, 2);

	fs["rgb_distortion"] >> coeffs;
	memcpy(data->color_intrinsic.coeffs, coeffs.data, sizeof(float) * 5);

	fs["depth_intrinsic"] >> k;
	data->depth_intrinsic.fx = k.at<float>(0, 0); data->depth_intrinsic.ppx = k.at<float>(0, 2);
	data->depth_intrinsic.fy = k.at<float>(1, 1); data->depth_intrinsic.ppy = k.at<float>(1, 2);

	fs["depth_distortion"] >> coeffs;
	memcpy(data->depth_intrinsic.coeffs, coeffs.data, sizeof(float) * 5);

	fs["depth_to_color_Rot"] >> R;
	memcpy(data->depth_to_color.rotation, R.data, sizeof(float) * 9);

	fs["depth_to_color_tvec"] >> t;
	memcpy(data->depth_to_color.translation, t.data, sizeof(float) * 3);

	fs.release();
	return;
};

void writeParametersYaml(std::string full_path, const struct _rgbd_parameters_ * data)
{
	cv::FileStorage fs(full_path, cv::FileStorage::WRITE);
	if (!fs.isOpened()) {
		throw std::runtime_error("file is not opened: " + full_path);
		return;
	}

	fs << "cam_name" << data->cam_id;
	cv::Mat k = cv::Mat::eye(3, 3, CV_32FC1);
	k.at<float>(0, 0) = data->color_intrinsic.fx; k.at<float>(0, 2) = data->color_intrinsic.ppx;
	k.at<float>(1, 1) = data->color_intrinsic.fy; k.at<float>(1, 2) = data->color_intrinsic.ppy;

	fs << "rgb_intrinsic" << k;
	fs << "rgb_distortion" << cv::Mat(1, 5, CV_32FC1, (float*)data->color_intrinsic.coeffs);

	k.at<float>(0, 0) = data->depth_intrinsic.fx; k.at<float>(0, 2) = data->depth_intrinsic.ppx;
	k.at<float>(1, 1) = data->depth_intrinsic.fy; k.at<float>(1, 2) = data->depth_intrinsic.ppy;
	fs << "depth_intrinsic" << k;
	fs << "depth_distortion" << cv::Mat(1, 5, CV_32FC1, (float*)data->depth_intrinsic.coeffs);
	fs << "depth_to_color_Rot" << cv::Mat(3, 3, CV_32FC1, (float*)data->depth_to_color.rotation);
	fs << "depth_to_color_tvec" << cv::Mat(3, 1, CV_32FC1, (float*)data->depth_to_color.translation);

	fs.release();
	return;
};

void readCameraOrder(std::string full_path, std::vector<std::string>& cam_order, int& ref_cam_idx)
{
	cv::FileStorage fs;
	if (!fs.open(full_path, cv::FileStorage::READ))
	{
		throw std::runtime_error("Could not open the cam_order:\n" + full_path);
		return;
	}

	int num_of_cam = (int)fs["num_of_cameras"];
	ref_cam_idx = (int)fs["ref_camera_idx"];
	std::vector<std::string> order(num_of_cam);
	if (num_of_cam == 3)
	{
		order[(int)fs["right"]] = "right";
		order[(int)fs["left"]] = "left";
		order[(int)fs["front"]] = "front";
	}
	else if (num_of_cam == 4)
	{
		order[(int)fs["front"]] = "front";
		order[(int)fs["left"]] = "left";
		order[(int)fs["right"]] = "right";
		order[(int)fs["rear"]] = "rear";
	}
	else
	{
		throw std::runtime_error("Unspecified type. Talk to the developer");
	}
	cam_order = order;
	return;
};