//
//    rfnoc-hls-neuralnet: Vivado HLS code for neural-net building blocks
//
//    Copyright (C) 2017 EJ Kreinar
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "firmware/myproject.h"
#include "firmware/nnet_utils/nnet_helpers.h"

//hls-fpga-machine-learning insert bram

#define CHECKPOINT 5000

namespace nnet {
    bool trace_enabled = true;
    std::map<std::string, void *> *trace_outputs = NULL;
    size_t trace_type_size = sizeof(double);
}

template<class T, size_t SIZE>
void load_weights_from_txt(T *w, const char* fname) {

    std::string full_path = std::string(WEIGHTS_DIR) + "/" + std::string(fname);
    std::ifstream infile(full_path.c_str(), std::ios::binary);

    if (infile.fail()) {
        std::cerr << "ERROR: file " << std::string(fname) << " does not exist" << std::endl;
        exit(1);
    }

    std::string line;
    if (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string token;

        size_t i = 0;
        while(std::getline(iss, token, ',')) {
            std::istringstream(token) >> w[i];
            i++;
        }

        if (SIZE != i) {
            std::cerr << "ERROR: Expected " << SIZE << " values";
            std::cerr << " but read only " << i << " values" << std::endl;
        }
    }
}



int main(int argc, char **argv)
{
  //load input data from text file
  std::ifstream fin("/home/YL_HUANG/10_28/PTQ/total_bits_32/tb_data/tb_input_features.dat");
  //load predictions from text file
  std::ifstream fpr("tb_data/tb_output_predictions.dat");

#ifdef RTL_SIM
  std::string RESULTS_LOG = "tb_data/rtl_cosim_results.log";
#else
  std::string RESULTS_LOG = "tb_data/csim_results.log";
#endif
  std::ofstream fout(RESULTS_LOG);

  std::string iline;
  std::string pline;
  int e = 0;

  weight18_t w18[36864];
  weight22_t w22[73728];
  weight25_t w25[147456];
  weight29_t w29[294912];
  weight32_t w32[589824];
  weight36_t w36[589824];
  weight39_t w39[65536];


  load_weights_from_txt<weight18_t, 36864>(w18, "w18.txt");
  load_weights_from_txt<weight22_t, 73728>(w22, "w22.txt");
  load_weights_from_txt<weight25_t, 147456>(w25, "w25.txt");
  load_weights_from_txt<weight29_t, 294912>(w29, "w29.txt");
  load_weights_from_txt<weight32_t, 589824>(w32, "w32.txt");
  load_weights_from_txt<weight36_t, 589824>(w36, "w36.txt");
  load_weights_from_txt<weight39_t, 65536>(w39, "w39.txt");

  if (fin.is_open() && fpr.is_open()) {
    while ( std::getline(fin,iline) && std::getline (fpr,pline) ) {
      if (e % CHECKPOINT == 0) std::cout << "Processing input " << e << std::endl;
      char* cstr=const_cast<char*>(iline.c_str());
      char* current;
      std::vector<float> in;
      current=strtok(cstr," ");
      while(current!=NULL) {
        in.push_back(atof(current));
        current=strtok(NULL," ");
      }
      cstr=const_cast<char*>(pline.c_str());
      std::vector<float> pr;
      current=strtok(cstr," ");
      while(current!=NULL) {
        pr.push_back(atof(current));
        current=strtok(NULL," ");
      }

      //hls-fpga-machine-learning insert data
      hls::stream<input_t> em_endcap[N_INPUT_3_1];
      nnet::copy_data_switch<float, input_t, 0, N_INPUT_1_1*N_INPUT_2_1*N_INPUT_3_1,N_INPUT_3_1>(in, em_endcap);
      hls::stream<result_t> layer44_out[1];

      //hls-fpga-machine-learning insert top-level-function
      unsigned short size_in1,size_out1;
      myproject(em_endcap,layer44_out,w18,w22,w25,w29,w32,w36,w39);

      if (e % CHECKPOINT == 0) {
        std::cout << "Predictions" << std::endl;
        //hls-fpga-machine-learning insert predictions
        for(int i = 0; i < N_LAYER_42; i++) {
          std::cout << pr[i] << " ";
        }
        std::cout << std::endl;
        std::cout << "Quantized predictions" << std::endl;
        //hls-fpga-machine-learning insert quantized
        nnet::print_result_switch<result_t, N_LAYER_42, 1>(layer44_out, std::cout, true);
      }
      e++;

      //hls-fpga-machine-learning insert tb-output
      nnet::print_result_switch<result_t, N_LAYER_42, 1>(layer44_out, fout);

    }
    fin.close();
    fpr.close();
  } 

  fout.close();
  std::cout << "INFO: Saved inference results to file: " << RESULTS_LOG << std::endl;

  return 0;
}