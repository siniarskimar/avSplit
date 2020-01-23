#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <unordered_map>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/avstring.h>
	#include <libavcodec/avcodec.h>
}
void printUsage() {
	std::cout << "Usage: \n"
			<< "\t avSplit [filename]"
			<< std::endl;
}
#define RETURN_FATAL -1
#define RETURN_USER_ERROR 1
#define RETURN_SUCCESS 0
#define ALLOC_ERROR -100

std::string getFirstToken(const std::string& tokenStr) {
	std::string token = tokenStr;
	auto delimPos = token.find(',');
	if(delimPos != std::string::npos) {
		token = token.substr(0, delimPos);
	}
	return token;
}
int open_input(AVFormatContext** context, const std::string& filename) {
	int res = 0;
	if(res = avformat_open_input(context, filename.c_str(), NULL, NULL) < 0) {
		return res;
	}
	if(res = avformat_find_stream_info(*context, NULL) < 0) {
		return res;
	}
	return res;
}
int new_output_stream_from_input(AVFormatContext* context, AVStream** outStream, AVStream* inStream) {
	*outStream = avformat_new_stream(context, NULL);
	if(outStream == NULL) {
		return ALLOC_ERROR;
	}
	int res = 0;
	if(res = avcodec_parameters_copy((*outStream)->codecpar, inStream->codecpar) < 0) {
		return res;
	}
	return res;
}
int alloc_output_context(AVFormatContext** context, AVOutputFormat* format = NULL, const std::string& filename = "", const std::string& formatName = "") {
	if(format == NULL) {
		format = av_guess_format(
			formatName.size() ? formatName.c_str() : NULL,
			filename.size() ? filename.c_str() : NULL,
			NULL
		);
	}

	int res = 0;
	if(res = avformat_alloc_output_context2(context, format, formatName.c_str(), filename.c_str()) < 0) {
		return res;
	}
	return res;
}
int open_output_io(AVFormatContext* context, const std::string& filename) {
	int res = 0;
	if(!(context->oformat->flags & AVFMT_NOFILE)) {
		if(res = avio_open(&(context->pb), filename.c_str(), AVIO_FLAG_WRITE) < 0) {
			return res;
		}
	}
	return res;
}


int main(int argc, const char* argv[]) {
	if(argc < 2) {
		printUsage();
		return RETURN_USER_ERROR;
	}
	int err = 0;
	AVFormatContext* inputContext = avformat_alloc_context();
	if(inputContext == NULL) {
		std::cerr << "Error allocating input context" << std::endl;
		return RETURN_FATAL;
	}
	if(open_input(&inputContext, argv[1]) < 0) {
		std::cout << "Failed opening the file '" << argv[1] << "'\n"
			<< "Please check if the file exists" << std::endl;
		avformat_close_input(&inputContext);
		avformat_free_context(inputContext);
		return RETURN_USER_ERROR;
	}
	

	av_dump_format(inputContext, 0, argv[1], 0);
	std::unordered_map<int, AVFormatContext*> outputContexts;
	outputContexts.reserve(inputContext->nb_streams);

	for(unsigned int i = 0; i < inputContext->nb_streams; i++) {
		AVStream*& inStream = inputContext->streams[i];
		AVFormatContext* outContext = NULL;
		AVOutputFormat* outFormat = NULL;

		AVCodec* codec = avcodec_find_decoder(inStream->codecpar->codec_id);
		if(codec == NULL)
			codec = avcodec_find_encoder(inStream->codecpar->codec_id);
		std::stringstream ssFilename;
		std::string formatName = getFirstToken(inputContext->iformat->name);

		ssFilename << "stream" << inStream->id 
			<< '.' << ( codec != NULL ? getFirstToken(codec->name) : "unknown")
			<< "." << formatName;
		std::string filename = ssFilename.str();

		if(alloc_output_context(&outContext, outFormat, filename, formatName) < 0) {
			std::cerr << "Error allocating output context for stream " << i << std::endl;
			continue;
		}
		AVStream* outStream = NULL;
		if(err = new_output_stream_from_input(outContext, &outStream, inStream) < 0) {
			if(err == ALLOC_ERROR) {
				std::cerr << "Failed allocating output stream for stream" << i;
			} else {
				std::cerr << "Failed copying codec parameters for stream" << i;
			}
			std::cerr << std::endl;
			avformat_free_context(outContext);
			continue;
		}
		if(open_output_io(outContext, filename) < 0) {
			std::cerr << "Failed opening output io for stream" << i << std::endl;

			avformat_free_context(outContext);
			continue;
		}
		if(int res = avformat_write_header(outContext, NULL) < 0) {
			std::cerr << "Failed writing file header for stream " << i << std::endl;
			avio_close(outContext->pb);
			avformat_free_context(outContext);
			continue;
		}

		outputContexts.insert({i, outContext});
	}
	AVPacket* packet = av_packet_alloc();
	while(true) {
		int ret = av_read_frame(inputContext, packet);
		if(ret == AVERROR_EOF) {
			break;
		} else if (ret < 0) {
			std::cerr << "Error reading a frame" << std::endl;
			break;
		}
		auto itFormat = outputContexts.find(packet->stream_index);
		if(itFormat == outputContexts.end()) {
			av_packet_unref(packet);
			continue;
		}

		AVFormatContext*& outContext = (*itFormat).second;
		AVStream*& inStream = inputContext->streams[packet->stream_index];
		AVStream*& outStream = outContext->streams[0];

		int64_t roundingInt = AV_ROUND_PASS_MINMAX | AV_ROUND_NEAR_INF;

		// Temporary workaround for "int cannot be casted into AVRounding"
		AVRounding rounding = (*reinterpret_cast<AVRounding*>(&roundingInt));

		packet->stream_index = outStream->index;
		packet->pts = av_rescale_q_rnd(packet->pts, inStream->time_base, outStream->time_base, rounding);
		packet->dts = av_rescale_q_rnd(packet->dts, inStream->time_base, outStream->time_base, rounding);
		packet->duration = av_rescale_q(packet->duration, inStream->time_base, outStream->time_base);

		if(ret = av_interleaved_write_frame(outContext, packet) < 0) {
			std::cerr << "Error writing a packet" << std::endl;
		}

		av_packet_unref(packet);
	}
	av_packet_free(&packet);

	for(auto& contextPair : outputContexts) {
		av_write_trailer(contextPair.second);

		avio_close(contextPair.second->pb);
		avformat_free_context(contextPair.second);
	}

	avformat_close_input(&inputContext);
	avformat_free_context(inputContext);
	
	return RETURN_SUCCESS;
}