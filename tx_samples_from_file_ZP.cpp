//
// Copyright 2010-2012,2014 Ettus Research LLC
// Author: Peng Zheng
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "usrp_cal_utils.hpp"
#include <uhd/exception.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/utils/thread.hpp>
#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <fstream> 
#include <string>
#include <thread>

namespace po = boost::program_options;

/***********************************************************************
 * Signal handlers
 **********************************************************************/
static bool stop_signal_called = false;
void sig_int_handler(int)
{
    stop_signal_called = true;
}

/***********************************************************************
 * Main function
 **********************************************************************/
int UHD_SAFE_MAIN(int argc, char* argv[])
{
    // variables to be set by po
    std::string args, file, type, ant, subdev, ref, pps, otw, channel_list;
    uint64_t total_num_samps;
    size_t spb;
    double rate, freq, gain, power, bw, lo_offset;
   

    // setup the program options
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
	("help", "help message, Author: Peng Zheng ")
	("args", po::value<std::string>(&args)->default_value(""), "single uhd device address args")
	("file", po::value<std::string>(&file)->default_value("usrp_samples.dat"), "name of the file to read binary samples from")
	("type", po::value<std::string>(&type)->default_value("short"), "sample type: double, float, or short which are based on your input data file")
	("spb", po::value<size_t>(&spb)->default_value(0), "samples per buffer, 0 for default")
        ("rate", po::value<double>(&rate), "rate of outgoing samples")
        ("freq", po::value<double>(&freq), "RF center frequency in Hz")
        ("lo-offset", po::value<double>(&lo_offset)->default_value(0.0),
            "Offset for frontend LO in Hz (optional)")
        ("gain", po::value<double>(&gain), "gain for the RF chain")
        ("ant", po::value<std::string>(&ant), "antenna selection")
        ("subdev", po::value<std::string>(&subdev), "subdevice specification")
        ("bw", po::value<double>(&bw), "analog frontend filter bandwidth in Hz")
        ("ref", po::value<std::string>(&ref)->default_value("internal"), "clock reference (internal, external, mimo, gpsdo)")
        ("pps", po::value<std::string>(&pps), "PPS source (internal, external, mimo, gpsdo)")
        ("otw", po::value<std::string>(&otw)->default_value("sc16"), "specify the over-the-wire sample mode")
        ("channels", po::value<std::string>(&channel_list)->default_value("0"), "which channels to use (specify \"0\", \"1\", \"0,1\", etc)")
        ("int-n", "tune USRP with integer-N tuning???which can help to reduce spur performance at the low freq")
    ;
    // clang-format on
    po::variables_map vm;
	// ???????????????variables_map????????????map??? ???????????????????????????????????????????????????????????????
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
	// ??????store,parse_command_line???notify??????????????????????????????????????????vm???
	// https://blog.csdn.net/qq_15457239/article/details/51179434
	// ??????boost????????????????????? https://blog.csdn.net/huang714/article/details/104749007/
    // print the help message
    if (vm.count("help")) {
        std::cout << boost::format("UHD TX Waveforms %s") % desc << std::endl;
        return ~0;
    }

    // create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args
              << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);
	// usrp??????????????????????????????
	
    // always select the subdevice first, the channel mapping affects the other settings
    if (vm.count("subdev"))
        usrp->set_tx_subdev_spec(subdev);

    // detect which channels to use
    std::vector<std::string> channel_strings; // channel_strings??????string???????????????
    std::vector<size_t> channel_nums; // channel_nums?????? size_t???????????????
    boost::split(channel_strings, channel_list, boost::is_any_of("\"',"));
	
	//c++????????????boost:split?????????????????????????????????????????????????????????????????????\???
	// ???channel list?????????????????????????????????channel_strings
	// ???0 \ 0,1 \ 0,1,2 == 0   0,1   0,1,2
	
    for (size_t ch = 0; ch < channel_strings.size(); ch++) {
		// verctor.size() ????????????
        size_t chan = std::stoi(channel_strings[ch]);
		// stoi ?????????????????? ??????????????????????????????n??????????????? n ???????????????????????????????????? 
        if (chan >= usrp->get_tx_num_channels())
            throw std::runtime_error("Invalid channel(s) specified.");
        else
            channel_nums.push_back(std::stoi(channel_strings[ch]));
		// .push_back ??????????????????channel_nums???????????? channel_string[ch]?????????????????????stoi????????????
		// ???????????????????????????channel list ????????????channel_strings ??????channel_strings??????10???????????????
		// channel_nums
    }
	std::cout << std::endl;
	std::cout << std::endl;
	
	// std::<< boost::format("channel_nums value is ") % (channel_nums) << std::endl;
	//std::cout << "channel_nums value is " << channel_nums << "!" << std::endl;
	// ??????????????????????????????
	std::cout << boost::format("channel_nums value is %f ") % usrp->get_tx_num_channels() << std::endl;
	std::cout << std::endl;
	
    // Lock mboard clocks
    if (vm.count("ref")) {
        usrp->set_clock_source(ref); //usrp???sptr???->????????????usrp???????????????????????????????????????
    }

    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    // set the sample rate
    if (not vm.count("rate")) {
        std::cerr << "Please specify the sample rate with --rate" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting TX Rate: %f Msps...") % (rate / 1e6) << std::endl;
    usrp->set_tx_rate(rate);
    std::cout << boost::format("Actual TX Rate: %f Msps...") % (usrp->get_tx_rate() / 1e6)
              << std::endl
              << std::endl;

    // set the center frequency
    if (not vm.count("freq")) {
        std::cerr << "Please specify the center frequency with --freq" << std::endl;
        return ~0;
    }


    for (size_t ch = 0; ch < channel_nums.size(); ch++) {
        std::cout << boost::format("Setting TX Freq: %f MHz...") % (freq / 1e6)
                  << std::endl;
        std::cout << boost::format("Setting TX LO Offset: %f MHz...") % (lo_offset / 1e6)
                  << std::endl;
        uhd::tune_request_t tune_request(freq, lo_offset);
        if (vm.count("int-n")) {
            tune_request.args = uhd::device_addr_t("mode_n=integer");
        usrp->set_tx_freq(tune_request, channel_nums[ch]);
        std::cout << boost::format("Actual TX Freq: %f MHz...")
                         % (usrp->get_tx_freq(channel_nums[ch]) / 1e6)
                  << std::endl
                  << std::endl;
}
	// ZP: ????????????vector channel_nums???????????????
	// ?????????????????????????????????
	std::cout << "Element of channel_nums is " << channel_nums.at(ch)<< "!" << std::endl
                  << std::endl;
				  
        // set the rf gain
        if (vm.count("gain")) {
			std::cout << boost::format("Setting TX Gain: %f dB...") % gain << std::endl;
            usrp->set_tx_gain(gain, channel_nums[ch]);
            std::cout << boost::format("Actual TX Gain: %f dB...")
                             % usrp->get_tx_gain(channel_nums[ch])
                      << std::endl
                      << std::endl;
       }

        // set the analog frontend filter bandwidth
        if (vm.count("bw")) {
            std::cout << boost::format("Setting TX Bandwidth: %f MHz...") % bw
                      << std::endl;
            usrp->set_tx_bandwidth(bw, channel_nums[ch]);
            std::cout << boost::format("Actual TX Bandwidth: %f MHz...")
                             % usrp->get_tx_bandwidth(channel_nums[ch])
                      << std::endl
                      << std::endl;
        }

        // set the antenna
        if (vm.count("ant"))
            usrp->set_tx_antenna(ant, channel_nums[ch]);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1)); // allow for some setup time

    // create a transmit streamer with specified data type
    // data format 
	// fc32: data format in cpu memory, fc32 = complex<float>, CPU format ????????? APP ??????
	// otw: data format over the wire
	/*  - sc16 - Q16 I16
        - sc8 - Q8_1 I8_1 Q8_0 I8_0
        - sc12 (Only some devices) */
        
	std::string data_format;
	if (type == "double")
        data_format = "fc64";
    else if (type == "float")
        data_format = "fc32";
    else if (type == "short")
        data_format = "sc16";
    uhd::stream_args_t stream_args(data_format, otw);
     
    stream_args.channels             = channel_nums;
	// stream_args ???struct ??? channels ????????? vector<size_t>
	
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

    // allocate a buffer which we re-use for each channel
    // spb: samples per buffer
    if (spb == 0) {
        spb = tx_stream->get_max_num_samps() * 10;
    }
    std::vector<std::complex<float>> buff(spb);
    std::vector<std::complex<float>*> buffs(channel_nums.size(), &buff.front());
	
//	ZP infile ?????????????????????
	std::ifstream infile(file.c_str(), std::ifstream::binary);
    // pre-fill the buffer with the waveform
    for (size_t n = 0; n < buff.size(); n++) {
		// wave_table ?????????public??????????????????
		infile.read((char*)&buff[n], buff.size() * sizeof(samp_type));
// ZP ??????????????????buffer????????????
    }

    std::cout << boost::format("Setting device timestamp to 0...") << std::endl;
	// channel??????????????????channel_nums???????????????????????????????????????????????????
	
    if (channel_nums.size() > 1) {
        // Sync times
        if (pps == "mimo") {
            UHD_ASSERT_THROW(usrp->get_num_mboards() == 2);

            // make mboard 1 a slave(??????) over the MIMO Cable
            usrp->set_time_source("mimo", 1);

            // set time on the master (mboard 0)
            usrp->set_time_now(uhd::time_spec_t(0.0), 0);

            // sleep a bit while the slave locks its time to the master
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            if (pps == "internal" or pps == "external" or pps == "gpsdo")
                usrp->set_time_source(pps);
            usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
            std::this_thread::sleep_for(
                std::chrono::seconds(1)); // wait for pps sync pulse
        }
    } else {
        usrp->set_time_now(0.0);
    }

    // Check Ref and LO Lock detect
    std::vector<std::string> sensor_names;
    const size_t tx_sensor_chan = channel_nums.empty() ? 0 : channel_nums[0];
    sensor_names                = usrp->get_tx_sensor_names(tx_sensor_chan);
    if (std::find(sensor_names.begin(), sensor_names.end(), "lo_locked")
        != sensor_names.end()) {
        uhd::sensor_value_t lo_locked = usrp->get_tx_sensor("lo_locked", tx_sensor_chan);
        std::cout << boost::format("Checking TX: %s ...") % lo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(lo_locked.to_bool());
    }
    const size_t mboard_sensor_idx = 0;
    sensor_names                   = usrp->get_mboard_sensor_names(mboard_sensor_idx);
    if ((ref == "mimo")
        and (std::find(sensor_names.begin(), sensor_names.end(), "mimo_locked")
                != sensor_names.end())) {
        uhd::sensor_value_t mimo_locked =
            usrp->get_mboard_sensor("mimo_locked", mboard_sensor_idx);
        std::cout << boost::format("Checking TX: %s ...") % mimo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(mimo_locked.to_bool());
    }
    if ((ref == "external")
        and (std::find(sensor_names.begin(), sensor_names.end(), "ref_locked")
                != sensor_names.end())) {
        uhd::sensor_value_t ref_locked =
            usrp->get_mboard_sensor("ref_locked", mboard_sensor_idx);
        std::cout << boost::format("Checking TX: %s ...") % ref_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(ref_locked.to_bool());
    }

    std::signal(SIGINT, &sig_int_handler);
    std::cout << "Press Ctrl + C to stop streaming..." << std::endl;

    // Set up metadata. We start streaming a bit in the future
    // to allow MIMO operation:
    uhd::tx_metadata_t md;
    md.start_of_burst = true;
    md.end_of_burst   = false;
    md.has_time_spec  = true;
    md.time_spec      = usrp->get_time_now() + uhd::time_spec_t(0.1);

    // send data until the signal handler gets called
    // or if we accumulate the number of samples specified (unless it's 0)
    //????????????????????????????????????????????????????????????????????????????????????????????? 0??? 
    uint64_t num_acc_samps = 0;
    while (true) {
        // Break on the end of duration or CTRL-C
        if (stop_signal_called) {
            break;
        }
        // Break when we've received nsamps
        if (total_num_samps > 0 and num_acc_samps >= total_num_samps) {
            break;
        }

        // send the entire contents of the buffer
        num_acc_samps += tx_stream->send(buffs, buff.size(), md); //????????????


        // fill the buffer with the waveform
        for (size_t n = 0; n < buff.size(); n++) {
		// wave_table ?????????public??????????????????
		infile.read((char*)&buff[n], buff.size() * sizeof(samp_type));
// ZP ??????????????????buffer????????????
  		  }

        md.start_of_burst = false;
        md.has_time_spec  = false;
    }

    // send a mini EOB packet
    md.end_of_burst = true;
    tx_stream->send("", 0, md);

    // finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;
    return EXIT_SUCCESS;
}

