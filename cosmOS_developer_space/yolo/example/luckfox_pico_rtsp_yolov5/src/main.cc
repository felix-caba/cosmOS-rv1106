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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "rtsp_demo.h"
#include "luckfox_mpi.h"
#include "yolov5.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

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

int main(int argc, char *argv[])
{
	system("RkLunch-stop.sh");
	int16_t detection_output_fd = -1;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--detection-fd") == 0)
		{
			if (i + 1 < argc)
			{
				detection_output_fd = atoi(argv[i + 1]);
				i++;
				fprintf(stderr, "YOLO: Detection output will be sent to FD: %d\n", detection_output_fd); // Log to stderr
			}
			else
			{
				fprintf(stderr, "YOLO: --detection-fd option requires a value.\n");
			}
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

	// rtsp init
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;
	// g_rtsplive = create_rtsp_demo(554);
	// g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

	// vi init
	vi_dev_init();
	vi_chn_init(0, width, height);

	// venc init
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
	venc_init(0, width, height, enCodecType);

	printf("venc init success\n");

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

			cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
			cv::resize(bgr, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

			// letterbox
			cv::Mat letterboxImage = letterbox(frame);
			memcpy(rknn_app_ctx.input_mems[0]->virt_addr, letterboxImage.data, model_width * model_height * 3);
			inference_yolov5_model(&rknn_app_ctx, &od_results);

			for (int i = 0; i < od_results.count; i++)
			{
				if (od_results.count >= 1)
				{
					object_detect_result *det_result = &(od_results.results[i]);

					sX = (int)(det_result->box.left);
					sY = (int)(det_result->box.top);
					eX = (int)(det_result->box.right);
					eY = (int)(det_result->box.bottom);
					mapCoordinates(&sX, &sY);
					mapCoordinates(&eX, &eY);
					////////////////////////////////////////////////////////////////////////
					if (detection_output_fd != -1)
					{ // Only if FD is valid
						char detection_line_buffer[256];
						int len = snprintf(detection_line_buffer, sizeof(detection_line_buffer),
										   "%s\n", coco_cls_to_name(det_result->cls_id));

						if (len > 0 && len < (int)sizeof(detection_line_buffer))
						{
							ssize_t written_bytes = write(detection_output_fd, detection_line_buffer, len);
							if (written_bytes == -1)
							{
								fprintf(stderr, "YOLO: Error writing to detection_output_fd (FD: %d). Return value: %zd, errno: %d (%s)\n",
										detection_output_fd, written_bytes, errno, strerror(errno));
							}
							else if (written_bytes < len)
							{
								fprintf(stderr, "YOLO: Partial write to detection_output_fd\n");
							}
						}
						else if (len >= (int)sizeof(detection_line_buffer))
						{
							fprintf(stderr, "YOLO: detection_line_buffer too small for snprintf output.\n");
						}
						else
						{
							fprintf(stderr, "YOLO: snprintf error for detection line.\n");
						}
					}
					////////////////////////////////////////////////////////////////
					cv::rectangle(frame, cv::Point(sX, sY),
								  cv::Point(eX, eY),
								  cv::Scalar(0, 255, 0), 3);
					sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
					cv::putText(frame, text, cv::Point(sX, sY - 8),
								cv::FONT_HERSHEY_SIMPLEX, 1,
								cv::Scalar(0, 255, 0), 2);
				}
			}
		}
		memcpy(data, frame.data, width * height * 3);

		// encode H264
		RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if (s32Ret == RK_SUCCESS)
		{
			void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
		}

		static int web_frame_counter = 0;
		if (++web_frame_counter % 2 == 0)
		{ // Every 2nd frame instead of 3rd = faster updates
			std::vector<uchar> buffer;
			cv::imencode(".jpg", frame, buffer, std::vector<int>{
													cv::IMWRITE_JPEG_QUALITY, 40, // Lower quality = faster encoding
												});

			// Atomic write to prevent torn reads
			FILE *jpg = fopen("/tmp/frame_new.jpg", "wb");
			if (jpg)
			{
				fwrite(buffer.data(), 1, buffer.size(), jpg);
				fclose(jpg);
				rename("/tmp/frame_new.jpg", "/tmp/frame.jpg"); // Atomic replace
			}
		}

		// release frame
		s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		if (s32Ret != RK_SUCCESS)
		{
			RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
		}
		s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
		if (s32Ret != RK_SUCCESS)
		{
			RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
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

	if (g_rtsplive)
		rtsp_del_demo(g_rtsplive);

	RK_MPI_SYS_Exit();

	// Release rknn model
	release_yolov5_model(&rknn_app_ctx);
	deinit_post_process();

	return 0;
}
