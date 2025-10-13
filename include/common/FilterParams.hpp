#ifndef FILTERPARAMS
#define FILTERPARAMS




struct AudioFilterParams {
    double volume = 1.0;       
    double tempo = 1.0;       
    bool enable_echo = false;   // 是否开启回声
    double echo_in_delay = 0.1; // 回声输入延迟(ms)
    double echo_in_decay = 0.1; // 回声衰减
    double echo_out_delay = 0.1;
    double echo_out_decay = 0.1;
    bool enable_lowpass = false;
    double lowpass_freq = 0.0;  // 低通频率
    bool enable_highpass = false;
    double highpass_freq = 0.0; // 高通频率
    bool mute = false;
};


struct VideoFilterParams {
    float brightness = 0.0f;   //亮度 -1.0 ~ 1.0
    float contrast = 1.0f;     //对比度 0.0 ~ 2.0
    float saturation = 1.0f;   //饱和度 0.0 ~ 2.0
    int rotate = 0;            //旋转角度 0 / 90 / 180 / 270

    float hue = 0.0f;          // -1.0 ~ 1.0, 调整色调
    bool enable_grayscale = false; // 是否启用灰度滤镜
    bool enable_sepia = false;    // 是否启用怀旧色调（Sepia）
    int blur_radius = 0;          // 模糊半径（0代表禁用）

};


#endif