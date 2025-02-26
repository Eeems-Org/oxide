#pragma once
// This file is missing from the SDK.
// It's a recreation based on the disassembly of the library.
#include <QImage>
#include <QRect>
#include <QFlags>

#define EPFR_SIZE 0x110
#define EPFR_OFFSET_MAINBUFFER 0xc8
#define EPFR_OFFSET_AUXBUFFER 0xa8
#define EPFR_SIZE_RM2 0x88
#define EPFR_OFFSET_MAINBUFFER_RM2 0x70
#define EPFR_OFFSET_AUXBUFFER_RM2 0x60

#ifdef __aarch64__
#define DEFAULT_EPFR_RETURN EPFramebufferAcep2
#else
#define DEFAULT_EPFR_RETURN EPFramebufferFusion
#endif

enum EPScreenMode {
    QualityFastest = 0,
    QualityFast = 1,
    Quality3 = 3,
    QualityFull = 4,
    Quality5 = 5,
};

enum EPContentType {
    Mono = 0,
    Color = 1,
};

class EPFramebuffer {
public:
    enum UpdateFlag {
        NoRefresh = 0,
        CompleteRefresh = 1,
    };
    void setBuffers(std::tuple<QImage, QImage>, QImage *old=nullptr);
    unsigned long swapBuffers(QRect param_1, EPContentType epct, EPScreenMode type, QFlags<EPFramebuffer::UpdateFlag> flags);
    static class DEFAULT_EPFR_RETURN *instance();
};

class EPFramebufferSwtcon : public EPFramebuffer{
public:
    void initialize(void);
    void sync(void);
    // unsigned long update(QRect param_1, int color, PixelMode type, int fullRefresh);
};

#ifdef __aarch64__
class EPFramebufferAcep2 : public EPFramebufferSwtcon {
public:
    EPFramebufferAcep2();
    int sendTModeUpdate(void);
private:
    char OPAQUE_A[EPFR_OFFSET_AUXBUFFER];
public:
    QImage auxBuffer;
private:
    char OPAQUE_B[EPFR_OFFSET_MAINBUFFER - sizeof(QImage) - EPFR_OFFSET_AUXBUFFER];
public:
    QImage mainBuffer;
private:
    char OPAQUE_C[EPFR_SIZE - EPFR_OFFSET_MAINBUFFER - sizeof(QImage)];
};
#endif

#ifdef __arm__
class EPFramebufferFusion : public EPFramebufferSwtcon {
public:
    EPFramebufferFusion();
private:
    char OPAQUE_A[EPFR_OFFSET_AUXBUFFER_RM2];
public:
    QImage auxBuffer;
private:
    char OPAQUE_B[EPFR_OFFSET_MAINBUFFER_RM2 - sizeof(QImage) - EPFR_OFFSET_AUXBUFFER_RM2];
public:
    QImage mainBuffer;
private:
    char OPAQUE_C[EPFR_SIZE_RM2 - EPFR_OFFSET_MAINBUFFER_RM2 - sizeof(QImage)];
};
#endif
