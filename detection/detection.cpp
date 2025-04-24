// head_detect.cpp
// Standalone C++ program using OpenCV to detect enemy heads and draw outlines
// Usage: head_detect <input_image> <output_image>

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <corecrt_math_defines.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_image> <output_image>\n";
        return -1;
    }

    // Load image
    cv::Mat img = cv::imread(argv[1]);
    if (img.empty()) {
        std::cerr << "Error: could not load image '" << argv[1] << "'\n";
        return -1;
    }

    // Resize ROI if needed
    // cv::resize(img, img, cv::Size(200,200));

    // 1) Convert to grayscale and equalize histogram
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    // 2) Smooth and detect edges
    cv::Mat blur, edges;
    cv::GaussianBlur(gray, blur, cv::Size(5,5), 1.5);
    cv::Canny(blur, edges, 50, 150);

    // 3) HoughCircles for circular head detection
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(
        blur, circles,
        cv::HOUGH_GRADIENT,
        1,      // dp
        20,     // minDist between centers
        100,    // Canny high threshold
        30,     // accumulator threshold
        8,      // minRadius
        30      // maxRadius
    );

    // 4) Contour fallback with circularity filter
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    struct Candidate { cv::Point2f center; float circ; };
    std::vector<Candidate> candidates;

    for (auto &C : contours) {
        double area = cv::contourArea(C);
        if (area < 100 || area > 5000) continue;
        double perimeter = cv::arcLength(C, true);
        if (perimeter <= 0) continue;
        double circ = 4 * M_PI * area / (perimeter * perimeter);
        if (circ < 0.5) continue;
        cv::Moments m = cv::moments(C);
        if (m.m00 == 0) continue;
        cv::Point2f c(m.m10/m.m00, m.m01/m.m00);
        candidates.push_back({c, (float)circ});
    }

    // 5) Draw results on a copy
    cv::Mat output = img.clone();
    // Draw Hough circles in green
    for (auto &v : circles) {
        cv::Point center(cvRound(v[0]), cvRound(v[1]));
        int radius = cvRound(v[2]);
        cv::circle(output, center, radius, cv::Scalar(0,255,0), 2);
        cv::circle(output, center, 3, cv::Scalar(0,255,0), -1);
    }
    // Draw contour-based candidates in blue
    for (auto &cand : candidates) {
        cv::circle(output, cand.center, 15, cv::Scalar(255,0,0), 2);
    }

    // Save output
    if (!cv::imwrite(argv[2], output)) {
        std::cerr << "Error: could not write output '" << argv[2] << "'\n";
        return -1;
    }

    std::cout << "Detection complete. Output saved to " << argv[2] << "\n";
    return 0;
}

/* Build Instructions:
 * Requires OpenCV (>=3.0)
 * g++ head_detect.cpp -o head_detect `pkg-config --cflags --libs opencv4`
 * Usage: ./head_detect frame_00031.jpg out.jpg
 */
