#include "Sound.h"
#include <cassert>

Sound* Sound::GetInstance() {
    static Sound instance;
    return &instance;
}

void Sound::Initialize() {
    HRESULT hr;

    // XAudio2エンジンのインスタンスを作成
    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    // マスターボイスを作成
    hr = xAudio2_->CreateMasteringVoice(&masteringVoice_);
    assert(SUCCEEDED(hr));
}

void Sound::Finalize() {
    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    xAudio2_.Reset();
}

Sound::SoundData Sound::SoundLoadWave(const std::string& filename) {
    // 1. ファイルオープン
    std::ifstream file;
    file.open(filename, std::ios::binary);
    assert(file.is_open());

    // 2. RIFFヘッダの読み込み
    RiffHeader riff;
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
        assert(false && "Not a valid WAV file.");
    }

    // 3. チャンクのシークと fmt チャンクの読み込み
    FormatChunk format{};
    while (file.good()) {
        ChunkHeader chunk;
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));

        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            assert(chunk.size <= sizeof(format.fmt));
            format.chunk = chunk;
            file.read(reinterpret_cast<char*>(&format.fmt), chunk.size);
            break;
        }
        else {
            // 目当てのチャンクでなければサイズ分スキップ
            file.seekg(chunk.size, std::ios::cur);
        }
    }

    // 4. data チャンクの読み込み
    ChunkHeader data;
    while (file.good()) {
        file.read(reinterpret_cast<char*>(&data), sizeof(data));

        if (strncmp(data.id, "data", 4) == 0) {
            break;
        }
        else {
            file.seekg(data.size, std::ios::cur);
        }
    }

    // 5. データ読み込み用のバッファを確保して読み込む
    SoundData soundData{};
    soundData.wfex = format.fmt;
    soundData.bufferSize = data.size;
    soundData.pBuffer.resize(data.size);
    file.read(reinterpret_cast<char*>(soundData.pBuffer.data()), data.size);

    // ファイルを閉じる
    file.close();

    return soundData;
}

void Sound::SoundUnload(SoundData* soundData) {
    // std::vectorが自動解放するため、メモリのクリアのみ
    soundData->pBuffer.clear();
    soundData->bufferSize = 0;
}

void Sound::SoundPlayWave(const SoundData& soundData) {
    HRESULT hr;

    // ソースボイスの作成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr));

    // 再生する波形データの設定
    XAUDIO2_BUFFER buffer{};
    buffer.pAudioData = soundData.pBuffer.data();
    buffer.AudioBytes = soundData.bufferSize;
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データのサブミット
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    assert(SUCCEEDED(hr));

    // 再生開始
    hr = pSourceVoice->Start(0);
    assert(SUCCEEDED(hr));
}