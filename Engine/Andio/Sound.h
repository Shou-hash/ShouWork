#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <fstream>

class Sound
{
public:
    // チャンクヘッダ
    struct ChunkHeader {
        char id[4];     // チャンクUI
        int32_t size;   // チャンクサイズ
    };

    // RIFFヘッダ
    struct RiffHeader {
        ChunkHeader chunk;  // "RIFF"
        char type[4];       // "WAVE"
    };

    // fmtチャンク
    struct FormatChunk {
        ChunkHeader chunk;  // "fmt "
        WAVEFORMATEX fmt;   // 波形フォーマット
    };

    // 音声データ
    struct SoundData {
        WAVEFORMATEX wfex;       // 波形フォーマット
        std::vector<byte> pBuffer; // データの先頭アドレス (vectorで安全にメモリ管理)
        unsigned int bufferSize; // データのサイズ
    };

public:
    // シングルトンインスタンスの取得
    static Sound* GetInstance();

    // 初期化と終了処理
    void Initialize();
    void Finalize();

    // WAVファイルの読み込み
    SoundData SoundLoadWave(const std::string& filename);

    // 音声データの解放
    void SoundUnload(SoundData* soundData);

    // 音声再生
    void SoundPlayWave(const SoundData& soundData);

private:
    Sound() = default;
    ~Sound() = default;
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;
};