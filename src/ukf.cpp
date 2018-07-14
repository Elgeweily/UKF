#define M_pi 3.1415926535897

#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  is_initialized_ = false;

  // initialize time
  time_us_ = 0;

  // State dimension
  n_x_ = 5;

  // Augmented state dimension
  n_aug_ = 7;

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initialize state vector
  x_ = VectorXd(5);

  // initialize covariance matrix
  P_ = MatrixXd(5, 5);
  P_ = MatrixXd::Identity(5, 5);

  // initialize predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  //define spreading parameter
  lambda_ = 3 - n_aug_;

  // initialize weights of sigma points
  weights_ = VectorXd(2 * n_aug_ + 1);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 30; //probably needs to be set to 3

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 30; //probably needs to be set to 3
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /*****************************************************************************
  *  Initialization
  ****************************************************************************/
  if (!is_initialized_) {
    // first measurement
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
	  */
      float range = meas_package.raw_measurements_(0);
	  float bearing = meas_package.raw_measurements_(1);
  
      x_ << range * cos(bearing), range * sin(bearing), 0, 0, 0;
    }
	else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
	  /**
      Initialize state.
      */
      x_ << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), 0, 0, 0;
	}

    // update time
	time_us_ = meas_package.timestamp_;

	// done initializing, no need to predict or update
	is_initialized_ = true;
	return;
  }
  /*****************************************************************************
  *  Prediction
  ****************************************************************************/
  float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;	//dt - expressed in seconds
  time_us_ = meas_package.timestamp_;
  Prediction(dt);
  /*****************************************************************************
  *  Update
  ****************************************************************************/
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    // radar updates
	if (use_radar_) {
	  UpdateRadar(meas_package);
	}
  }
  else {
    // laser updates
	if (use_laser_) {
	  UpdateLidar(meas_package);
	}

	// print the output
	cout << "x_ = " << x_ << endl;
	cout << "P_ = " << P_ << endl;
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  /*****************************************************************************
  *  Sigma Points Generation
  ****************************************************************************/
  // local variables
  MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);
  MatrixXd A = P_.llt().matrixL();

  // calculate and set sigma points
  Xsig.col(0) = x_;
  MatrixXd intermediate = sqrt(3) * A;
  Xsig.block<5, 5>(0, 1) = intermediate.colwise() + x_;
  intermediate *= -1;
  Xsig.block<5, 5>(0, 6) = intermediate.colwise() + x_;
  /*****************************************************************************
  *  Augmentation
  ****************************************************************************/
  // local variables
  VectorXd x_aug = VectorXd(7);
  MatrixXd P_aug = MatrixXd(7, 7);
  MatrixXd Xsig_aug = MatrixXd(n_x_, 2 * n_aug_ + 1);

  // create augmented mean state
  x_aug.head(5) = x_;
  x_aug.tail(2) << 0, 0;
  // create augmented covariance matrix
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug.bottomRightCorner(2, 2) << pow(std_a_, 2), 0, 
	  0, pow(std_yawdd_, 2);
  // calculate square root matrix of P_aug
  MatrixXd A_aug = P_aug.llt().matrixL();
  // create augmented sigma points
  Xsig_aug.col(0) = x_aug;
  // set remaining sigma points
  for (int i = 0; i < n_aug_; i++)
  {
    Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * A_aug.col(i);
    Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A_aug.col(i);
  }
  /*****************************************************************************
  *  Sigma Points Prediction
  ****************************************************************************/
  for (int i = 0; i < Xsig_aug.cols(); i++) {
	// local variables
    VectorXd x = Xsig_aug.col(i).head(5);
	VectorXd noise = Xsig_aug.col(i).tail(2);
	VectorXd xsig_pred = VectorXd(5);

	if (x(4) != 0) {
	  xsig_pred(0) = x(0) + (x(2) / x(4)) * (sin(x(3) + x(4) * delta_t) - sin(x(3)))
		  + 0.5 * delta_t * delta_t * cos(x(3)) * noise(0);
	  xsig_pred(1) = x(1) + (x(2) / x(4)) * (-cos(x(3) + x(4) * delta_t) + cos(x(3)))
		  + 0.5 * delta_t * delta_t * sin(x(3)) * noise(0);
	  xsig_pred(2) = x(2) + 0 + delta_t * noise(0);
	  xsig_pred(3) = x(3) + x(4) * delta_t + 0.5 * delta_t * delta_t * noise(1);
	  xsig_pred(4) = x(4) + 0 + delta_t * noise(1);
	}
	else {
      xsig_pred(0) = x(0) + x(2) * cos(x(3) )* delta_t + 0.5 * delta_t * delta_t * cos(x(3)) * noise(0);
	  xsig_pred(1) = x(1) + x(2) * sin(x(3)) * delta_t + 0.5 * delta_t * delta_t* sin(x(3)) * noise(0);
	  xsig_pred(2) = x(2) + 0 + delta_t * noise(0);
	  xsig_pred(3) = x(3) + 0 + 0.5 * delta_t * delta_t * noise(1);
	  xsig_pred(4) = x(4) + 0 + delta_t * noise(1);
	}
	Xsig_pred_.col(i) = xsig_pred;
  }
  /*****************************************************************************
  *  Calculate Predicted Mean and Covariance
  ****************************************************************************/
  // local variables
  VectorXd x_pred = VectorXd(5);
  MatrixXd P_pred = MatrixXd(5, 5);
 
  // set weights
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for (int i = 1; i < 2 * n_aug_ + 1; i++) {
	  weights_(i) = 1 / (2 * (lambda_ + n_aug_));
  }
  // predict state mean
  x_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
	  x_pred += weights_(i) * Xsig_pred_.col(i);
  }
  // predict state covariance matrix
  P_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
	  VectorXd diff = VectorXd(n_x_);
	  diff = Xsig_pred_.col(i) - x_pred;

	  // angle normalization
	  while (diff(3) > M_pi) diff(3) -= 2 * M_pi;
	  while (diff(3) < -M_pi) diff(3) += 2 * M_pi;

	  P_pred += weights_(i) * diff * diff.transpose();
  }
  x_ = x_pred;
  P_ = P_pred;
}
 
/**
* Updates the state and the state covariance matrix using a radar measurement.
* @param {MeasurementPackage} meas_package
*/
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  /*****************************************************************************
  *  Measurement Prediction
  ****************************************************************************/
  // local variables
  int n_z = 2;
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  VectorXd z_pred = VectorXd(n_z);
  MatrixXd S_pred = MatrixXd(n_z, n_z);
  MatrixXd R = MatrixXd(n_z, n_z);

  // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    float px = Xsig_pred_(0, i);
	float py = Xsig_pred_(1, i);

	Zsig(0, i) = px;
	Zsig(1, i) = py;
  }
  // calculate mean predicted measurement
  z_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
	  z_pred += weights_(i) * Zsig.col(i);
  }
  // calculate innovation covariance matrix S
  S_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd diff = (Zsig.col(i) - z_pred);
	S_pred += weights_(i) * diff * diff.transpose();
  }
  R << std_radr_ * std_radr_, 0, 0, 
	  0, std_radphi_ * std_radphi_, 0,
	  0, 0, std_radrd_ * std_radrd_;
  S_pred += R;
  /*****************************************************************************
  *  Measurement Update
  ****************************************************************************/
  // local variables
  VectorXd z = meas_package.raw_measurements_;
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //calculate cross correlation matrix
  Tc.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;
	VectorXd x_diff = Xsig_pred_.col(i) - x_;

	// angle normalization
	while (x_diff(3) > M_pi) x_diff(3) -= 2 * M_pi;
	while (x_diff(3) < -M_pi) x_diff(3) += 2 * M_pi;

	Tc += weights_(i) * x_diff * z_diff.transpose();
  }
  //calculate Kalman gain K;
  MatrixXd K = Tc * S_pred.inverse();
  //update state mean and covariance matrix
  x_ = x_ + K * (z - z_pred);
  P_ = P_ - K * S_pred * K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

  /*****************************************************************************
  *  Measurement Prediction
  ****************************************************************************/
  // local variables
  int n_z = 3;
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  VectorXd z_pred = VectorXd(n_z);
  MatrixXd S_pred = MatrixXd(n_z, n_z);
  MatrixXd R = MatrixXd(n_z, n_z);

  // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
	float px = Xsig_pred_(0, i);
	float py = Xsig_pred_(1, i);
	float v = Xsig_pred_(2, i);
	float phi = Xsig_pred_(3, i);

	Zsig(0, i) = sqrt(pow(px, 2) + pow(py, 2));
	Zsig(1, i) = atan2(py, px);
	Zsig(2, i) = ((px * cos(phi) * v + py * sin(phi) * v) / sqrt(pow(px, 2) + pow(py, 2)));
  }
  // calculate mean predicted measurement
  z_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred += weights_(i) * Zsig.col(i);
  }
  // calculate innovation covariance matrix S
  S_pred.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd diff = (Zsig.col(i) - z_pred);

    // angle normalization
	while (diff(1) > M_pi) diff(1) -= 2 * M_pi;
	while (diff(1) < -M_pi) diff(1) += 2 * M_pi;

	S_pred += weights_(i) * diff * diff.transpose();
  }
  R << std_radr_ * std_radr_, 0, 0, 
	  0, std_radphi_ * std_radphi_, 0, 
	  0, 0, std_radrd_ * std_radrd_;
  S_pred += R;
  /*****************************************************************************
  *  Measurement Update
  ****************************************************************************/
  // local variables
  VectorXd z = meas_package.raw_measurements_;
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //calculate cross correlation matrix
  Tc.setZero();
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;

	// angle normalization
	while (z_diff(1) > M_pi) z_diff(1) -= 2 * M_pi;
	while (z_diff(1) < -M_pi) z_diff(1) += 2 * M_pi;

	VectorXd x_diff = Xsig_pred_.col(i) - x_;

	// angle normalization
	while (x_diff(3) > M_pi) x_diff(3) -= 2 * M_pi;
	while (x_diff(3) < -M_pi) x_diff(3) += 2 * M_pi;

	Tc += weights_(i) * x_diff * z_diff.transpose();
  }
  //calculate Kalman gain K;
  MatrixXd K = Tc * S_pred.inverse();
  //update state mean and covariance matrix
  x_ = x_ + K * (z - z_pred);
  P_ = P_ - K * S_pred * K.transpose();
}
