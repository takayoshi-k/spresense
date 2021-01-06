/*
 * This code is refered from https://hwswsgps.hatenablog.com/entry/2018/08/19/172401
 */
#include <stdio.h>
#include <stdlib.h>

/* Sould be defined as short cap_data[] in this hedare file */
#include "caotured_data.h"

typedef struct
{
  int fs; //サンプリング周波数
  int bits; //量子化bit数
  int L; //データ長
} WAV_PRM;

void audio_write(short *data, WAV_PRM *prm, char *filename)
{

  //変数宣言
  FILE *fp;
  int n;
  char header_ID[4];
  long header_size;
  char header_type[4];
  char fmt_ID[4];
  long fmt_size;
  short fmt_format;
  short fmt_channel;
  long fmt_samples_per_sec;
  long fmt_bytes_per_sec;
  short fmt_block_size;
  short fmt_bits_per_sample;
  char data_ID[4];
  long data_size;
 
  //ファイルオープン
  fp = fopen(filename, "wb");

  //ヘッダー書き込み
  header_ID[0] = 'R';
  header_ID[1] = 'I';
  header_ID[2] = 'F';
  header_ID[3] = 'F';
  header_size = 36 + prm->L * 2;
  header_type[0] = 'W';
  header_type[1] = 'A';
  header_type[2] = 'V';
  header_type[3] = 'E';
  fwrite(header_ID, 1, 4, fp);
  fwrite(&header_size, 4, 1, fp);
  fwrite(header_type, 1, 4, fp);
 
  //フォーマット書き込み
  fmt_ID[0] = 'f';
  fmt_ID[1] = 'm';
  fmt_ID[2] = 't';
  fmt_ID[3] = ' ';
  fmt_size = 16;
  fmt_format = 1;
  fmt_channel = 1;
  fmt_samples_per_sec = prm->fs;
  fmt_bytes_per_sec = prm->fs * prm->bits / 8;
  fmt_block_size = prm->bits / 8;
  fmt_bits_per_sample = prm->bits;
  fwrite(fmt_ID, 1, 4, fp);
  fwrite(&fmt_size, 4, 1, fp);
  fwrite(&fmt_format, 2, 1, fp);
  fwrite(&fmt_channel, 2, 1, fp);
  fwrite(&fmt_samples_per_sec, 4, 1, fp);
  fwrite(&fmt_bytes_per_sec, 4, 1, fp);
  fwrite(&fmt_block_size, 2, 1, fp);
  fwrite(&fmt_bits_per_sample, 2, 1, fp);
 
  //データ書き込み
  data_ID[0] = 'd';
  data_ID[1] = 'a';
  data_ID[2] = 't';
  data_ID[3] = 'a';
  data_size = prm->L * 2;
  fwrite(data_ID, 1, 4, fp);
  fwrite(&data_size, 4, 1, fp);

  //音声データ書き込み
  fp = fopen(filename, "wb");
	fwrite(data, 2, prm->L, fp);
 
  fclose(fp);
}

int main(void)
{
  WAV_PRM prm;

  prm.fs = 16000;
  prm.bits = 16;
  prm.L = 16000;

	audio_write(cap_data, &prm, "yes_master.wav");

  return 0;
}

