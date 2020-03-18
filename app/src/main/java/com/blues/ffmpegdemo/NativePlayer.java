package com.blues.ffmpegdemo;

public class NativePlayer {

    static {
        System.loadLibrary("bluesffmpeg");
    }

    public native static void playVideo(String url, Object surface);
}
