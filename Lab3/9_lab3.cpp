#include <iostream>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fstream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

struct framebuffer_info
{
    uint32_t bits_per_pixel;    // framebuffer depth
    uint32_t xres_virtual;      // how many pixel in a row in virtual screen
};

struct framebuffer_info get_framebuffer_info(const char *framebuffer_device_path);

int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int main(int argc, const char *argv[])
{
    cv::VideoCapture cap(2);
    if (!cap.isOpened()){
	    std::cout << "Cannot open camera\n";
        return 1;
    }

    cv::Mat frame;
    cv::Mat frame2;
    cv::Size2f frame_size;

    framebuffer_info fb_info = get_framebuffer_info("/dev/fb0");
    std::ofstream ofs("/dev/fb0");
    char c;
    char filename[100];
    int count = 1;

    while (true) {
        bool ret = cap.read(frame);
        if (!ret) {
		std::cout << "Can't receive frame\n";
            break;
        }

        frame_size = frame.size();
        cv::cvtColor(frame, frame2, cv::COLOR_BGR2BGR565);
        int offset = (fb_info.xres_virtual - frame_size.width) / 2;

        for (int y = 0; y < frame_size.height; y++) {
            ofs.seekp((y * fb_info.xres_virtual + offset) *
                      fb_info.bits_per_pixel / 8);
            ofs.write((char *) (frame2.ptr(y)),
                      frame_size.width * fb_info.bits_per_pixel / 8);
        }

        if (kbhit()) {
            c = getchar();
            if (c == 'q')
                break;
            else if (c == 'c') {
                snprintf(filename, sizeof(filename),
                         "/run/media/mmcblk1p2/images/frame_%d.bmp", count++);
                cv::imwrite(filename, frame);
            }
        }
    }

    cap.release();
    return 0;
}

struct framebuffer_info get_framebuffer_info(const char *framebuffer_device_path)
{
    struct framebuffer_info fb_info;        // Used to return the required attrs.
    struct fb_var_screeninfo screen_info;   // Used to get attributes of the device from OS kernel.
    // open device with linux system call "open()"
    // https://man7.org/linux/man-pages/man2/open.2.html
    int fd = open(framebuffer_device_path, O_RDWR);
    if (fd < 0) {
        puts("open error!\n");
        exit(1);
    }

    // get attributes of the framebuffer device thorugh linux system call "ioctl()".
    // the command you would need is "FBIOGET_VSCREENINFO"
    // https://man7.org/linux/man-pages/man2/ioctl.2.html
    // https://www.kernel.org/doc/Documentation/fb/api.txt
    int val = ioctl(fd, FBIOGET_VSCREENINFO, &screen_info);
    if (val) {
        puts("ioctl error!\n");
        exit(1);
    }

    // put the required attributes in variable "fb_info" you found with "ioctl() and return it."
    fb_info.xres_virtual = screen_info.xres_virtual; 
    fb_info.bits_per_pixel = screen_info.bits_per_pixel;

    return fb_info;
};
