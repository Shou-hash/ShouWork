#include "Sound.h"
#include <cassert>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <algorithm>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

Sound* Sound::GetInstance() {
    static Sound instance;
    return &instance;
}

void Sound::Initialize() {
    // シーンの作り直しで何度も呼ばれるので、初期化済みなら何もしない
    if (xAudio2_) {
        return;
    }

    HRESULT hr;

    // XAudio2エンジンのインスタンスを作成
    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    // マスターボイスを作成
    hr = xAudio2_->CreateMasteringVoice(&masteringVoice_);
    assert(SUCCEEDED(hr));

    hr = MFStartup(MF_VERSION);
    assert(SUCCEEDED(hr));
}

void Sound::Finalize() {

    for (auto* v : voices_) { v->DestroyVoice(); }
    voices_.clear();

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

    // 再生終了したボイスを破棄
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
        [](IXAudio2SourceVoice* v) {
            XAUDIO2_VOICE_STATE st{};
            v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (st.BuffersQueued == 0) { v->DestroyVoice(); return true; }
            return false;
        }), voices_.end());

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr));

    XAUDIO2_BUFFER buffer{};
    buffer.pAudioData = soundData.pBuffer.data();
    buffer.AudioBytes = soundData.bufferSize;
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    assert(SUCCEEDED(hr));
    hr = pSourceVoice->Start(0);
    assert(SUCCEEDED(hr));

    voices_.push_back(pSourceVoice);
}

Sound::SoundData Sound::SoundLoad(const std::string& filename) {
    auto pos = filename.find_last_of('.');
    std::string ext = (pos == std::string::npos) ? "" : filename.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".wav") ? SoundLoadWave(filename) : SoundLoadMF(filename);
}

Sound::SoundData Sound::SoundLoadMF(const std::string& filename) {
    using Microsoft::WRL::ComPtr;
    HRESULT hr;

    // UTF-8 -> wstring
    int len = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0);
    std::wstring wpath(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, wpath.data(), len);

    // SourceReader 作成
    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    assert(SUCCEEDED(hr));

    // 出力を PCM に指定(圧縮形式はここでデコードされる)
    const DWORD kStream = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    ComPtr<IMFMediaType> pcmType;
    MFCreateMediaType(&pcmType);
    pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = reader->SetCurrentMediaType(kStream, nullptr, pcmType.Get());
    assert(SUCCEEDED(hr));

    // 実際に決まった PCM フォーマットを取得
    ComPtr<IMFMediaType> outType;
    reader->GetCurrentMediaType(kStream, &outType);
    WAVEFORMATEX* wf = nullptr;
    UINT32 wfSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(outType.Get(), &wf, &wfSize);
    assert(SUCCEEDED(hr));

    SoundData soundData{};
    soundData.wfex = *wf;
    CoTaskMemFree(wf);

    // 全サンプルを読み切って連結
    while (true) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(kStream, 0, nullptr, &flags, nullptr, &sample);
        assert(SUCCEEDED(hr));
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buf;
        sample->ConvertToContiguousBuffer(&buf);
        BYTE* p = nullptr;
        DWORD size = 0;
        buf->Lock(&p, nullptr, &size);
        soundData.pBuffer.insert(soundData.pBuffer.end(), p, p + size);
        buf->Unlock();
    }
    soundData.bufferSize = static_cast<unsigned int>(soundData.pBuffer.size());
    return soundData;
}