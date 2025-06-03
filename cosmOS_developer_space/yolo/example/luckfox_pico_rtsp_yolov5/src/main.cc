#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include "rtsp_demo.h"
#include "luckfox_mpi.h"
#include "yolov5.h"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <map>

using namespace std;

#define YOLO_LOG_FILE "/tmp/yolo_log.log"

#define DISP_WIDTH 720
#define DISP_HEIGHT 480

// disp size
int width = DISP_WIDTH;
int height = DISP_HEIGHT;

// model size
int model_width = 640;
int model_height = 640;
float scale;
int leftPadding;
int topPadding;

static bool web_streaming_enabled = false;
static int web_frame_counter = 0;

static int jpeg_pipe_fd = -1;
static bool jpeg_streaming_initialized = false;

static vector<string> previous_detections;
static vector<string> current_detections;
static char detection_buffer[512]; // Buffer estatico para no estar todo el rato allocando memoria

void handle_web_streaming(const cv::Mat &frame)
{
	if (!web_streaming_enabled)
	{
		return; // No hacer nada si web streaming está deshabilitado
	}

	if (++web_frame_counter % 1 == 0)
	{							   // cada frame (siempre se ejecuta porque % 1 == 0)
		std::vector<uchar> buffer; // buffer para almacenar los datos jpeg comprimidos
		std::vector<int> jpeg_params = {
			cv::IMWRITE_JPEG_QUALITY, 85, // parametros de compresion jpeg al 85% de calidad
		};

		if (cv::imencode(".jpg", frame, buffer, jpeg_params))
		{ // convertir frame a jpeg
			// crear archivo temporal para escritura atomica
			FILE *jpg = fopen("/tmp/frame_new.jpg", "wb");
			if (jpg)
			{
				fwrite(buffer.data(), 1, buffer.size(), jpg); // escribir datos jpeg al archivo
				fclose(jpg);
				rename("/tmp/frame_new.jpg", "/tmp/frame.jpg"); // renombre para que sea atomico

				// el rename hace que se bloquee el acceso al archivo hasta que se complete la escritura.
				// por eso hago un new, hasta que frame.jpg no esté 100% escrito, no se puede acceder a él.

				// Si el fwrite tardase mucho la api leeria algo incompleto. escribiendo y luego rename
				// se evita que se lea un frame incompleto. lo unico malo, es que puede haber
				// delays entre frames si el procesamiento es lento, pero no se pierde calidad.

				static int log_counter = 0;
				if (++log_counter % 60 == 0)
				{ // log cada 60 frames pa no saturar
					printf("Updated frame.jpg (%zu bytes) - frame %d\n", buffer.size(), log_counter);
				}
			}
			else
			{
				static int error_count = 0;
				if (++error_count % 100 == 1)
				{ // reportar error cada 100 fallos
					printf("Failed to open /tmp/frame_new.jpg for writing (error %d)\n", error_count);
				}
			}
		}
		else
		{
			static int encode_error_count = 0;
			if (++encode_error_count % 100 == 1)
			{ // reportar error de codificacion cada 100 fallos
				printf("Failed to encode JPEG (error %d)\n", encode_error_count);
			}
		}
	}
}

cv::Mat letterbox(cv::Mat input)
{
	float scaleX = (float)model_width / (float)width;
	float scaleY = (float)model_height / (float)height;
	scale = scaleX < scaleY ? scaleX : scaleY;

	int inputWidth = (int)((float)width * scale);
	int inputHeight = (int)((float)height * scale);

	leftPadding = (model_width - inputWidth) / 2;
	topPadding = (model_height - inputHeight) / 2;

	cv::Mat inputScale;
	cv::resize(input, inputScale, cv::Size(inputWidth, inputHeight), 0, 0, cv::INTER_LINEAR);
	cv::Mat letterboxImage(640, 640, CV_8UC3, cv::Scalar(0, 0, 0));
	cv::Rect roi(leftPadding, topPadding, inputWidth, inputHeight);
	inputScale.copyTo(letterboxImage(roi));

	return letterboxImage;
}

void mapCoordinates(int *x, int *y)
{
	int mx = *x - leftPadding;
	int my = *y - topPadding;

	*x = (int)((float)mx / scale);
	*y = (int)((float)my / scale);
}

void log_error(const char *message)
{
	FILE *log_file = fopen(YOLO_LOG_FILE, "a");
	if (log_file)
	{
		fprintf(log_file, "%s\n", message);
		fclose(log_file);
	}
	else
	{
		fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
	}
}

bool get_current_detections_optimized(object_detect_result_list *od_results, vector<string> &detections)
{
    detections.clear();

    if (od_results == nullptr || od_results->count == 0)
    {
        return false;
    }

    // memoria por si acaso.
    if (detections.capacity() < (size_t)od_results->count)
    {
        detections.reserve(od_results->count);
    }

    std::map<string, int> object_counts;
    
    for (int i = 0; i < od_results->count; i++)
    {
        object_detect_result *det_result = &(od_results->results[i]);
        const char *object_name = coco_cls_to_name(det_result->cls_id);
        object_counts[object_name]++;
    }

    // strings con numero.
    for (const auto &pair : object_counts)
    {
        for (int i = 1; i <= pair.second; i++)
        {
            char numbered_object[32];
            snprintf(numbered_object, sizeof(numbered_object), "%s %d", pair.first.c_str(), i);
            detections.emplace_back(numbered_object);
        }
    }

    return !detections.empty();
}

int write_to_pipe_optimized(int pipe_fd, const vector<string> &detections)
{
	if (pipe_fd < 0 || detections.empty())
	{
		return -1;
	}

	int pos = 0;

	// primera
	int written = snprintf(detection_buffer + pos, sizeof(detection_buffer) - pos, "%s", detections[0].c_str());
	if (written < 0 || written >= (int)(sizeof(detection_buffer) - pos))
		return -1;
	pos += written;

	for (size_t i = 1; i < detections.size(); i++)
	{
		int remaining = sizeof(detection_buffer) - pos - 2; // -2 para ':' y '\n'
		if (remaining <= 0)
			break;

		written = snprintf(detection_buffer + pos, remaining, ":%s", detections[i].c_str());
		if (written < 0 || written >= remaining)
			break;
		pos += written;
	}

	// newline al final
	if (pos < (int)sizeof(detection_buffer) - 1)
	{
		detection_buffer[pos++] = '\n';
		detection_buffer[pos] = '\0';
	}

	ssize_t bytes_written = write(pipe_fd, detection_buffer, pos);
	return (bytes_written == pos) ? 0 : -1;
}

bool vectors_equal_fast(const vector<string> &a, const vector<string> &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	// Busqueda lineal pa conjuntos pequeñoides
	for (const string &item_a : a)
	{
		bool found = false;
		for (const string &item_b : b)
		{
			if (item_a == item_b)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[])
{
	// create log file

	FILE *log_file = fopen(YOLO_LOG_FILE, "w");
	if (log_file)
	{
		fprintf(log_file, "YOLOv5 Server Log\n");
		fclose(log_file);
	}
	else
	{
		fprintf(stderr, "Failed to create log file: %s\n", strerror(errno));
		return -1;
	}

	int16_t detection_output_fd = -1; // El pipe

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--detection-fd") == 0)
		{
			if (i + 1 < argc)
			{
				detection_output_fd = atoi(argv[i + 1]);
				i++;
				fprintf(stderr, "YOLO: Detection output will be sent to FD: %d\n", detection_output_fd);
			}
			else
			{
				fprintf(stderr, "YOLO: --detection-fd option requires a value.\n");
			}
		}
		else if (strcmp(argv[i], "--web") == 0)
		{
			web_streaming_enabled=true;
		}
	}

	RK_S32 s32Ret = 0;
	int sX, sY, eX, eY;

	// Rknn model
	char text[16];
	rknn_app_context_t rknn_app_ctx;
	object_detect_result_list od_results;
	int ret;
	const char *model_path = "./model/yolov5.rknn";
	memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
	init_yolov5_model(model_path, &rknn_app_ctx);
	printf("init rknn model success!\n");
	init_post_process();

	// h264_frame
	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	RK_U64 H264_PTS = 0;
	RK_U32 H264_TimeRef = 0;
	VIDEO_FRAME_INFO_S stViFrame;

	// Create Pool
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3;
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
	// PoolCfg.bPreAlloc = RK_FALSE;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
	printf("Create Pool success !\n");

	// Get MB from Pool
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

	// Build h264_frame
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888;
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
	cv::Mat frame(cv::Size(width, height), CV_8UC3, data);

	// rkaiq init
	RK_BOOL multi_sensor = RK_FALSE;
	const char *iq_dir = "/etc/iqfiles";
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	// hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);

	// rkmpi init
	if (RK_MPI_SYS_Init() != RK_SUCCESS)
	{
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}

	// vi init
	vi_dev_init();
	vi_chn_init(0, width, height);

	// venc init
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
	venc_init(0, width, height, enCodecType);

	printf("venc init success\n");
	static int web_frame_counter = 0;
	while (1)
	{
		// get vi frame
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs();
		s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
		if (s32Ret == RK_SUCCESS)
		{
			void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

			cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
			cv::Mat bgr(height, width, CV_8UC3, data);

			cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2RGB);
			cv::resize(bgr, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

			// letterbox
			cv::Mat letterboxImage = letterbox(frame);
			memcpy(rknn_app_ctx.input_mems[0]->virt_addr, letterboxImage.data, model_width * model_height * 3);
			inference_yolov5_model(&rknn_app_ctx, &od_results);

			if (od_results.count > 0)
			{
				bool has_detections = get_current_detections_optimized(&od_results, current_detections);

				if (has_detections && detection_output_fd != -1)
				{
					if (!vectors_equal_fast(current_detections, previous_detections))
					{
						if (write_to_pipe_optimized(detection_output_fd, current_detections) == 0)
						{
							previous_detections = current_detections; // Swap 
						}
						else
						{
							log_error("Failed to write detections to pipe");
						}
					}
				}

				for (int i = 0; i < od_results.count; i++)
				{ // Escritura de el frame
					object_detect_result *det_result = &(od_results.results[i]);

					sX = (int)(det_result->box.left);
					sY = (int)(det_result->box.top);
					eX = (int)(det_result->box.right);
					eY = (int)(det_result->box.bottom);
					mapCoordinates(&sX, &sY);
					mapCoordinates(&eX, &eY);

					cv::rectangle(frame, cv::Point(sX, sY), cv::Point(eX, eY), cv::Scalar(0, 255, 0), 3);
					sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
					cv::putText(frame, text, cv::Point(sX, sY - 8), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
				}
			}
			else
			{
				if (!previous_detections.empty() && detection_output_fd != -1)
				{
					const char *clear_msg = "CLEAR\n";
					write(detection_output_fd, clear_msg, strlen(clear_msg));
					previous_detections.clear();
				}
			}
		}
		memcpy(data, frame.data, width * height * 3);

		handle_web_streaming(frame);

		// release frame
		s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		if (s32Ret != RK_SUCCESS)
		{
			RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
		}
		memset(text, 0, 8);
	}

	// Destory MB
	RK_MPI_MB_ReleaseMB(src_Blk);
	// Destory Pool
	RK_MPI_MB_DestroyPool(src_Pool);

	RK_MPI_VI_DisableChn(0, 0);
	RK_MPI_VI_DisableDev(0);

	SAMPLE_COMM_ISP_Stop(0);

	RK_MPI_VENC_StopRecvFrame(0);
	RK_MPI_VENC_DestroyChn(0);

	free(stFrame.pstPack);

	/*
	if (g_rtsplive)
		rtsp_del_demo(g_rtsplive);
	*/
	RK_MPI_SYS_Exit();

	// Release rknn model
	release_yolov5_model(&rknn_app_ctx);
	deinit_post_process();

	return 0;
}
