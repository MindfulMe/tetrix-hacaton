#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

// class UserFaceModel {
//   std 
// };

constexpr int im_size = 256;
// constexpr int between_eyes = im_size*0.5;

cv::Point2f model_left_eye_center(im_size*0.25, im_size*0.2);
cv::Point2f model_right_eye_center(im_size*0.75, im_size*0.2);
cv::Point2f model_mouth_center(im_size*0.5, im_size*0.7);

struct OneFaceModel {
  cv::Mat img;
  cv::Mat edges_cann;

  cv::Rect left_eye;
  cv::Rect right_eye;
  cv::Rect mouth;

  cv::Point left_eye_center;
  cv::Point right_eye_center;
  cv::Point mouth_center;
};

class FaceClassifier {
  private: std::map<std::string, OneFaceModel> face_base;
  private: float threshold = 0;

  public: void addPerson(OneFaceModel &face_model, std::string name) {
    this->face_base[name] = face_model;
  }

  public: std::map<std::string, float> getScores(OneFaceModel &face_model) {
    std::map<std::string, float> scores;

    for (auto &[k, v] : this->face_base) {
      auto score = this->getSingleScore(face_model, v);
      std::cout << k << ": " << score << std::endl;
      scores[k] = score;
    }

    return scores;
  }

  private: float getSingleScore(OneFaceModel &src, OneFaceModel &tmpl) {
    cv::Mat dist_tr, labels;
    cv::distanceTransform(tmpl.edges_cann, dist_tr, labels, cv::DIST_L2, 3);

    double score = cv::sum(cv::mean(dist_tr, src.edges_cann))[0];

    return score;
  }
};

class FaceModelGenerator {
  public: FaceModelGenerator() {
    if(!face_cascade.load(face_cascade_name))
      throw std::runtime_error("Cascade file not found: face");
    if(!eye_cascade.load(eye_cascade_name))
      throw std::runtime_error("Cascade file not found: eye");
    if(!smile_cascade.load(smile_cascade_name))
      throw std::runtime_error("Cascade file not found: smile");
  }

  public: OneFaceModel getFaceModel(cv::Mat &frame) {
    cv::Mat gray_frame;
    // src.copyTo(frame);

    cv::cvtColor(frame, gray_frame, CV_BGR2GRAY);
    cv::equalizeHist(gray_frame, gray_frame);

    std::vector<cv::Rect> faces;

    face_cascade.detectMultiScale(gray_frame, faces, 1.1, 2.0, 0|CV_HAAR_SCALE_IMAGE, cv::Size(30, 30));

    if (faces.size()) {
      auto &face_rect = faces[0];
      cv::Mat face_roi = frame(face_rect);
      cv::Mat gray_face_cut;
      cv::cvtColor(face_roi, gray_face_cut, CV_BGR2GRAY);
      cv::equalizeHist(gray_face_cut, gray_face_cut);

      std::vector<cv::Rect> eyes;
      std::vector<cv::Rect> smiles;
      eye_cascade.detectMultiScale(gray_face_cut, eyes, 1.1, 2.0, 0|CV_HAAR_SCALE_IMAGE, cv::Size(30, 30));
      smile_cascade.detectMultiScale(gray_face_cut, smiles, 1.1, 2.0, 0|CV_HAAR_SCALE_IMAGE, cv::Size(30, 30));
      
      cv::rectangle(frame, face_rect, cv::Scalar(0, 255, 0), 2);

      if (eyes.size() != 2) {
        std::cout << "Are you even a human?!?! (eyes: " << eyes.size() << ")" << std::endl;
        throw std::runtime_error("No eyes");
      } else {
        cv::Rect isect = (eyes[0] & eyes[1]);
        if (isect.width != 0 || isect.height != 0) {
          throw std::runtime_error("Not enough eyes");
        }

        for (auto &eye : eyes) {
          cv::rectangle(face_roi, eye, cv::Scalar(255, 0, 0), 2);
        }

        auto it = std::max_element(smiles.begin(), smiles.end(), [](auto &a, auto &b) {
          auto y1 = a.height/2 + a.y;
          auto y2 = b.height/2 + b.y;

          return (y1 < y2);
        });

        if (it != smiles.end()) {
          // Successfully got face
          cv::rectangle(face_roi, *it, cv::Scalar(255, 255, 0), 2);

          cv::Point2f e1_c(eyes[0].x+eyes[0].width/2, eyes[0].y+eyes[0].height/2);
          cv::Point2f e2_c(eyes[1].x+eyes[1].width/2, eyes[1].y+eyes[1].height/2);
          cv::Point2f m_c(it->x+it->width/2, it->y+it->height/2);

          if (e1_c.x > e2_c.x) {
            std::swap(eyes[0], eyes[1]);
            std::swap(e1_c, e2_c);
          }

          cv::line(face_roi, e1_c, e2_c, cv::Scalar(0, 255, 255), 2);
          cv::line(face_roi, e1_c, m_c, cv::Scalar(255, 0, 255), 2);
          cv::line(face_roi, e2_c, m_c, cv::Scalar(255, 0, 255), 2);

          // cv::Mat scale_eyes_aff = (cv::Mat_<float>(2, 3) << ;
          std::vector<cv::Point2f> src {
            e1_c, e2_c, m_c,
          };
          std::vector<cv::Point2f> dst {
            model_left_eye_center,
            model_right_eye_center,
            model_mouth_center,
          };

          cv::Mat cannonical_face;
          cv::Mat edges;
          cv::Canny(gray_face_cut, edges, 100, 128);

          cv::Mat affine_to_cann = cv::getAffineTransform(src, dst);
          cv::warpAffine(gray_face_cut, cannonical_face, affine_to_cann, cv::Size(im_size, im_size),
              CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS);
          cv::warpAffine(edges, edges, affine_to_cann, cv::Size(im_size, im_size),
              cv::INTER_NEAREST+CV_WARP_FILL_OUTLIERS);
          // std::cout << dst[0] << std::endl
          //           << dst[1] << std::endl
          //           << dst[2] << std::endl << std::endl;
          // std::cout << src[0] << std::endl
          //           << src[1] << std::endl
          //           << src[2] << std::endl << std::endl;
          // std::cout << affine_to_cann << std::endl;

          OneFaceModel face_model = {
            .img = face_roi,
            .edges_cann = edges,

            .left_eye = eyes[0],
            .right_eye = eyes[1],
            .mouth = *it,

            .left_eye_center = e1_c,
            .right_eye_center = e2_c,
            .mouth_center = m_c,
          };

          std::cout << "Got face" << std::endl;

          cv::imshow("Face", face_roi);
          cv::imshow("FaceG", edges);
          cv::imshow("FaceCan", cannonical_face);

          return face_model;
        }
        else
          throw std::runtime_error("Not mouth");
      }
    }
    else {
      cv::putText(frame, "No face", cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(0, 0, 255), 2);
      throw std::runtime_error("No face");
    }

    std::cout << "Fuck" << std::endl;
    throw std::runtime_error("No face");
  }

  private: std::string face_cascade_name = "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml";
  private: std::string eye_cascade_name = "/usr/share/opencv/haarcascades/haarcascade_eye_tree_eyeglasses.xml";
  private: std::string smile_cascade_name = "/usr/share/opencv/haarcascades/haarcascade_smile.xml";

  private: cv::CascadeClassifier face_cascade;
  private: cv::CascadeClassifier eye_cascade;
  private: cv::CascadeClassifier smile_cascade;
};

int main(int argc, char **argv) {
  FaceModelGenerator face_detector;

  cv::VideoCapture cap(0);
  if(!cap.isOpened())
    std::runtime_error("No camera");

  ::FaceClassifier face_classifier;

  cv::namedWindow("Cam", 1);
  cv::namedWindow("Face", 1);
  cv::namedWindow("FaceG", 1);

  int counter = 0;
  std::string names[] = {"Nick", "John", "Yaroslav", "Alex"};

  while(true)
  {
    cv::Mat frame;
    cap >> frame;

    try {
      auto face_model = face_detector.getFaceModel(frame);
      auto scores = face_classifier.getScores(face_model);

      cv::imshow("Cam", frame);
      auto key = cv::waitKey(0);
      if (key == 'a') {
        face_classifier.addPerson(face_model, names[counter++]);
        std::cout << "Added face: " << names[counter-1] << std::endl;
      } else if (key == ' ') {
        std::cout << "Item scores: ";
        for (auto &[name, score] : scores)
          std::cout << name << ": " << score << "; ";
        std::cout << std::endl;
      } else if (key == 27)
        break;
    }
    catch(...) {
      std::cout << "Face not detected" << std::endl;
    }

    cv::imshow("Cam", frame);
    if (cv::waitKey(30) == 27) {
      break;
    }
  }

  return 0;
}
