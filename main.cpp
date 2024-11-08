#include <atomic>
#include <chrono>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <windows.h>

#include "cmdline.h"

int restoreCursor(void) {
  HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleCursorPosition(hStdOut, COORD{0, 0});
  return 0;
}

int clearScr(void) {
  HANDLE hStdOut;
  hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (!GetConsoleMode(hStdOut, &mode)) {
    return ::GetLastError();
  }

  const DWORD originalMode = mode;
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hStdOut, mode)) {
    return ::GetLastError();
  }
  DWORD written = 0;
  PCWSTR sequence = L"\x1b[2J";

  if (!WriteConsoleW(hStdOut, sequence, (DWORD)(wcslen(sequence)), &written,
                     NULL)) {
    SetConsoleMode(hStdOut, originalMode);
    return ::GetLastError();
  }
  SetConsoleMode(hStdOut, originalMode);
  restoreCursor();

  return 0;
}

char RGB2Ascii(char &ch, int r, int g, int b, int alpha = 256) {
  auto str = std::string("$@b%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/"
                         "\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ");
  // 166 167 162
  auto len = str.length();
  auto gray = (int)(0.126 * r + 0.7152 * g + 0.0722 * b);
  auto unit = (256.0 + 1) / len;
  if ((int)(gray / unit) >= str.length() || (int)(gray / unit) < 0) {
    throw 0;
  }
  return ch = (0 == alpha ? ' ' : str.at((int)(gray / unit)));
}

std::string Frm2Ascii(cv::Mat const &frm) {
  CV_Assert(frm.depth() == CV_8U);

  static std::string str((frm.rows + 1) * frm.cols + 1, 0);
  int c = 0;
  // std::thread t[frm.total()];
  // int r = 0;

  for (int h = 0; h < frm.rows; h++) {
    for (int w = 0; w < frm.cols; w++) {
      cv::Vec3b rgb = frm.at<cv::Vec3b>(h, w);
      RGB2Ascii(str.at(c++), rgb.val[0], rgb.val[1], rgb.val[2]);
      // t[r++] = std::thread([&]() -> void {
      //  RGB2Ascii(str.at(c++), rgb.val[0], rgb.val[1], rgb.val[2]);
      // });
    }
    str.at(c++) = '\n';
  }
  // for (auto &jt : t) {
  //   if (jt.joinable()) {
  //     jt.join();
  //   } else {
  //     jt.detach();
  //   }
  // }

  return str;
}

cv::Mat Sharper(cv::Mat const &frm) {
  int h = frm.rows;
  int w = frm.cols;
  cv::Mat ret(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
  for (int i = 0; i < h - 1; i++) {
    for (int j = 0; j < w - 1; j++) {
      ret.at<cv::Vec3b>(i, j) =
          (frm.at<cv::Vec3b>(i, j) - frm.at<cv::Vec3b>(i + 1, j + 1)) +
          (frm.at<cv::Vec3b>(i + 1, j) - frm.at<cv::Vec3b>(i, j + 1));
    }
  }
  return ret;
}

void printScr() {}

void mainloop(cv::String fn, std::function<int(void)> clearScr, bool sharp,
              cv::Size resolution = cv::Size(960, 360)) {
  cv::VideoCapture vidc;
  if (fn.empty()) {
    vidc.open(0);
  } else {
    vidc.open(fn);
  }

  if (!vidc.isOpened()) {
    throw 0;
  }

  // std::cout << "h:" << vidc.get(cv::CAP_PROP_FRAME_HEIGHT);
  // std::cout << "w:" << vidc.get(cv::CAP_PROP_FRAME_WIDTH);

  cv::Mat frm;
  std::string strb1;
  std::string strb2;
  std::atomic_int cb = 1;
  std::atomic_int cw = 0;

  auto clocAscii = [&]() -> void {
    // cv::Size dsize(frm.cols * 0.8, frm.rows * 0.25);
    cv::Size dsize = resolution;
    cv::Mat nf(dsize, CV_8UC3);
    cv::resize(frm, nf, dsize, 0.5, 0.5, cv::INTER_CUBIC);

    if (sharp) {
      nf = Sharper(nf);
    }
    if (cb) {
      cw = 1;
      strb1 = Frm2Ascii(nf);
      cw = 0;
    } else {
      cw = 0;
      strb2 = Frm2Ascii(nf);
      cw = 0;
    }
    cb = !cb;
  };

  auto printScr = [&]() -> void {
    while (cw == cb) {
    }
    if (cb) {
      std::cout << strb2;
    } else {
      std::cout << strb1;
    }
    cb = !cb;
  };

  vidc >> frm;
  if (frm.empty()) {
    throw 0;
  }
  clocAscii();

  do {
    vidc >> frm;
    if (frm.empty()) {
      break;
    }

    clearScr();

    // std::thread tout(printScr);
    std::thread f(clocAscii);
    printScr();

    // tout.join();
    f.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

  } while (!frm.empty());
}

int main(int argc, char *argv[]) {
  std::ios::sync_with_stdio(false);
  std::cout.tie(0);
  std::cin.tie(0);

  cmdline::parser psr;
  psr.add("clearScr", '\0',
          "wether to clear Screen rather than move sursor to left most");
  psr.add("sharp", '\0', "wether to sharp the image");
  psr.add<std::string>("file", 'f', "file to be process", false, "");
  psr.parse_check(argc, argv);

  try {
    mainloop(psr.get<std::string>("file"),
             psr.exist("clearScr") ? clearScr : restoreCursor,
             psr.exist("sharp"));
  } catch (int e) {
    std::cerr << "file doesn't exist or have no permittion." << std::endl;
    return -1;
  }

  return 0;
}
