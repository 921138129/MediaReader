#include "pch.h"

using namespace MediaCaptureReader;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Storage;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media::Imaging;

TEST_CLASS(MediaReaderTests)
{
public:

    TEST_METHOD(CX_W_MediaReader_FromFile_Basics)
    {
        auto reader = Await(MediaReader::CreateFromPathAsync(L"ms-appx:///car.mp4"));

        // Test MediaReader state
        Assert::IsTrue(reader->CanSeek);
        Assert::AreEqual(31578911i64, reader->Duration.Duration);
        Assert::IsNotNull(reader->VideoStream);
        Assert::IsNotNull(reader->AudioStream);
        Assert::AreEqual(2u, reader->AllStreams->Size);

        //// TODO
        //// Test access to native IMFSourceReader
        //ComPtr<IMFSourceReaderEx> sourceReader;
        //CHK(GetNativeService<IMediaReaderNative>(reader)->GetSourceReader(&sourceReader));
        //Assert::IsNotNull(sourceReader.Get());

        // Read the first video frame
        Log() << "Reading sample";
        auto result = Await(reader->VideoStream->ReadAsync());

        // Save it as JPEG
        Log() << "Saving sample";
        auto folder = Await(KnownFolders::PicturesLibrary->CreateFolderAsync(L"MediaCaptureReaderTests", CreationCollisionOption::OpenIfExists));
        auto file = Await(folder->CreateFileAsync(L"CX_W_MediaReader_TestBasics.jpg", CreationCollisionOption::ReplaceExisting));
        Await(MediaSample2DEncoder::SaveToFileAsync(safe_cast<MediaSample2D^>(result->Sample), file, ImageCompression::Jpeg));
        Log() << L"Saved " << file->Path->Data();
    }

    TEST_METHOD(CX_W_MediaReader_FromFile_ReadVideoUntilEndOfFile)
    {
        auto reader = Await(MediaReader::CreateFromPathAsync(L"ms-appx:///car.mp4", AudioInitialization::Deselected, VideoInitialization::Nv12));
        Assert::IsNotNull(reader->VideoStream);
        Assert::IsNotNull(reader->AudioStream);
        Assert::AreEqual(2u, reader->AllStreams->Size);
        Assert::IsTrue(reader->VideoStream->IsSelected);
        Assert::IsFalse(reader->AudioStream->IsSelected);

        while (true)
        {
            auto result = Await(reader->VideoStream->ReadAsync());
            if (result->EndOfStream)
            {
                Log() << L"EndOfStream reached";
                break;
            }
            if (result->Error)
            {
                Log() << L"Error received 0x" << hex << uppercase << setw(8) << setfill(L'0') << result->ErrorCode << L" " << result->ErrorMessage->Data();
                Assert::Fail();
            }
            Assert::IsNotNull(result->Sample);
        }
    }

    TEST_METHOD(CX_W_MediaReader_FromFile_ReadAudioUntilEndOfFile)
    {
        auto reader = Await(MediaReader::CreateFromPathAsync(L"ms-appx:///car.mp4", AudioInitialization::Pcm, VideoInitialization::Deselected));
        Assert::IsNotNull(reader->VideoStream);
        Assert::IsNotNull(reader->AudioStream);
        Assert::AreEqual(2u, reader->AllStreams->Size);
        Assert::IsFalse(reader->VideoStream->IsSelected);
        Assert::IsTrue(reader->AudioStream->IsSelected);

        while (true)
        {
            auto result = Await(reader->AudioStream->ReadAsync());
            if (result->EndOfStream)
            {
                Log() << L"EndOfStream reached";
                break;
            }
            if (result->Error)
            {
                Log() << L"Error received 0x" << hex << uppercase << setw(8) << setfill(L'0') << result->ErrorCode << L" " << result->ErrorMessage->Data();
                Assert::Fail();
            }
            Assert::IsNotNull(result->Sample);
        }
    }

    //
    // Windows tests use NullMediaCapture as the real MediaCapture tries to pop up a consent UI which is nowhere to be seen
    // and cannot be automatically dismissed from within the tests
    //

    TEST_METHOD(CX_W_MediaReader_FromCapture_Basics)
    {
        Await(CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
            CoreDispatcherPriority::Normal,
            ref new DispatchedHandler([]()
        {
            auto settings = ref new MediaCaptureInitializationSettings();
            settings->StreamingCaptureMode = StreamingCaptureMode::Video;

            auto capture = NullMediaCapture::Create();
            Await(capture->InitializeAsync(settings));

            auto graphicsDevice = MediaGraphicsDevice::CreateFromMediaCapture(capture);

            auto previewProps = (VideoEncodingProperties^)capture->VideoDeviceController->GetMediaStreamProperties(MediaStreamType::VideoPreview);

            auto image = ref new SurfaceImageSource(previewProps->Width, previewProps->Height);

            auto imagePresenter = MediaSample2DPresenter::CreateFromSurfaceImageSource(
                image,
                graphicsDevice,
                previewProps->Width,
                previewProps->Height
                );

            auto panel = ref new SwapChainPanel();
            auto swapChainPresenter = MediaSample2DPresenter::CreateFromSwapChainPanel(
                panel,
                graphicsDevice,
                previewProps->Width,
                previewProps->Height
                );

            auto mediaReader = Await(MediaReader::CreateFromMediaCaptureAsync(capture));

            for (int n = 0; n < 3; n++)
            {
                Log() << "Displaying frame";
                auto result = Await(mediaReader->VideoStream->ReadAsync());
                auto sample = safe_cast<MediaSample2D^>(result->Sample);
                swapChainPresenter->Present(sample);
                imagePresenter->Present(sample);
            }
        })));
    }

    TEST_METHOD(CX_W_MediaReader_FromCapture_SaveBgra8ToJpeg)
    {
        auto capture = NullMediaCapture::Create();
        Await(capture->InitializeAsync());

        auto mediaReader = Await(MediaReader::CreateFromMediaCaptureAsync(capture, AudioInitialization::Deselected, VideoInitialization::Bgra8));

        Log() << "Capturing sample";
        auto result = Await(mediaReader->VideoStream->ReadAsync());
        auto sample = safe_cast<MediaSample2D^>(result->Sample);
        Assert::IsTrue(sample->Format == MediaSample2DFormat::Bgra8);

        Log() << "Saving sample";
        auto folder = Await(KnownFolders::PicturesLibrary->CreateFolderAsync(L"MediaCaptureReaderTests", CreationCollisionOption::OpenIfExists));
        auto file = Await(folder->CreateFileAsync(L"CX_W_MediaReader_FromCapture_SaveBgra8ToJpeg.jpg", CreationCollisionOption::ReplaceExisting));
        Await(MediaSample2DEncoder::SaveToFileAsync(sample, file, ImageCompression::Jpeg));
        Log() << L"Saved " << file->Path->Data();
    }

    TEST_METHOD(CX_W_MediaReader_FromCapture_SaveNv12ToJpeg)
    {
        auto capture = NullMediaCapture::Create();
        Await(capture->InitializeAsync());

        auto mediaReader = Await(MediaReader::CreateFromMediaCaptureAsync(capture, AudioInitialization::Deselected, VideoInitialization::Nv12));

        Log() << "Capturing sample";
        auto result = Await(mediaReader->VideoStream->ReadAsync());
        auto sample = safe_cast<MediaSample2D^>(result->Sample);
        Assert::IsTrue(sample->Format == MediaSample2DFormat::Nv12);

        Log() << "Saving sample";
        auto folder = Await(KnownFolders::PicturesLibrary->CreateFolderAsync(L"MediaCaptureReaderTests", CreationCollisionOption::OpenIfExists));
        auto file = Await(folder->CreateFileAsync(L"CX_W_MediaReader_FromCapture_SaveNv12ToJpeg.jpg", CreationCollisionOption::ReplaceExisting));
        Await(MediaSample2DEncoder::SaveToFileAsync(sample, file, ImageCompression::Jpeg));
        Log() << L"Saved " << file->Path->Data();
    }

};
