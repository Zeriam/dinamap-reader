#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* memset */
#include <plot.h>   /* gnu libplot */
#include <unistd.h>

/* binary block data structure */
typedef struct {
  char SeqNum;       /* block sequence number, modulo 200 */
  char WFStat;       /* waveform status */
  char WFData[2][5]; /* 2 = number of waveform channels */
  char NonWFData[3]; /* non-waveform parameter data and status */
  char CSum[2];      /* checksum */
  char ocoSeqNum;    /* one's complement of sequence number */
} BinBlk2Type;

void print_debug(BinBlk2Type *bblk, short *wfs) {
  printf("[DEBUG] bblk->SeqNum: %02hhx\n", bblk->SeqNum);
  printf("[DEBUG] bblk->WFStat: %02hhx\n", bblk->WFStat);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 5; ++j) {
      printf("[DEBUG] bblk->WFData[%d][%d]: %02hhx\n", i, j, bblk->WFData[i][j]);
    }
  }
  for (int i = 0; i < 3; ++i) {
    printf("[DEBUG] bblk->NonWFData[%d]: %02hhx\n", i, bblk->NonWFData[i]);
  }
  for (int i = 0; i < 4; ++i) {
    printf("[DEBUG] wfs[%d] = %hd\n", i, wfs[i]);
  }
  printf("[DEBUG] bblk->CSum[0]: %02hhx\n", bblk->CSum[0]);
  printf("[DEBUG] bblk->CSum[1]: %02hhx\n", bblk->CSum[1]);
  printf("[DEBUG] bblk->ocoSeqNum: %02hhx\n\n", bblk->ocoSeqNum);
}

void parse_waveform_status(BinBlk2Type *bblk, char *prev_wfstat, uint *qrs_events,
                           uint *num_breaths, uint *warn_alarms, uint *cris_alarms) {
  static char first_time = 1;
  if (first_time) {
    *prev_wfstat = bblk->WFStat;
    first_time = 0;
  }
  else {
    if ((bblk->WFStat & 0x6) != (*prev_wfstat & 0x6)) {
      ++*qrs_events;
    }
    if ((bblk->WFStat & 0x18) != (*prev_wfstat & 0x18)) {
      ++*num_breaths;
    }
    *prev_wfstat = bblk->WFStat; /* save WFStat for next invocation */
  }
  if (bblk->WFStat & 0x20) {
    printf("[ALERT] warning alarm detected!\n");
    ++*warn_alarms;
  }
  if (bblk->WFStat & 0x40) {
    printf("[ALERT] CRISIS ALARM DETECTED!\n");
    ++*cris_alarms;
  }
}

void unpack_waveform_samples(short *wfs, char *pwda) {
  wfs[0] = ((pwda[0] << 2) | ((pwda[1] >> 6) & 0x03)) & 0x03ff;
  wfs[1] = ((pwda[1] << 4) | ((pwda[2] >> 4) & 0x0f)) & 0x03ff;
  wfs[2] = ((pwda[2] << 6) | ((pwda[3] >> 2) & 0x3f)) & 0x03ff;
  wfs[3] = ((pwda[3] << 8) | ((pwda[4] >> 0) & 0xff)) & 0x03ff;
}

int main(int argc, char *argv[]) {
  const char *WF_DATA = "./wf_data.txt"; /* temp waveform data filename */
  const uint BLOCK_LEN = 34; /* length of block (as ascii) */
  char   DEBUG = 0;          /* optional debug level */
  char   prev_wfstat;        /* previous waveform status */
  short  wfs[4];             /* waveform samples: bits 15 to 10 are zero */
  char  *block = NULL;       /* buffer space for blocks */
  size_t len = 0;            /* read block length (as ascii) */
  float  x = 0.0, y = 0.0;   /* waveform coordinates (seconds, microvolts) */
  uint   i, j, k;            /* loop counters */
  uint   read_blocks = 0;    /* number of read blocks */
  uint   qrs_events  = 0;    /* number of detected QRS events */
  uint   num_breaths = 0;    /* number of detected breaths */
  uint   warn_alarms = 0;    /* number of detected warning alarms */
  uint   cris_alarms = 0;    /* number of detected crisis alarms */
  FILE  *fp_in = NULL;       /* input binary block data */
  FILE  *fp_wfdata = NULL;   /* temporary waveform data */
  BinBlk2Type *bblk = NULL;  /* binary block pointer */
  plPlotter *plotter = NULL; /* waveform plotter */
  plPlotterParams *plotter_params = pl_newplparams(); /* plotter parameters */

  if (argc < 2) {
    printf("usage: %s <input_filename> <optional debug level>\n", argv[0]);
    return 0;
  }
  if (argc > 2) DEBUG = atoi(argv[2]);
  if ((fp_in = fopen(argv[1], "r")) == NULL) {
    perror(argv[1]);
    return -1;
  }
  if ((fp_wfdata = fopen(WF_DATA, "w+")) == NULL) {
    perror(WF_DATA);
    return -1;
  }
  if ((bblk = (BinBlk2Type *)malloc(sizeof(BinBlk2Type))) == NULL) {
    perror("malloc");
    return -1;
  }
  memset(bblk, 0, sizeof(BinBlk2Type));

  /* read and process Dinamap Pro 1000 binary blocks */
  while (getline(&block, &len, fp_in) != -1) {
    if (len < BLOCK_LEN) {
      fprintf(stderr, "[WARNING] ignoring malformed block...\n");
      continue;
    }
    /* read block sequence number */
    sscanf(&block[0], "%2hhx", &bblk->SeqNum);
    /* read waveform status */
    sscanf(&block[2], "%2hhx", &bblk->WFStat);
    parse_waveform_status(bblk, &prev_wfstat, &qrs_events,
                          &num_breaths, &warn_alarms, &cris_alarms);
    /* read waveform data */
    for (i = 0, k = 4; i < 2; ++i) {
      for (j = 0; j < 5; ++j, k += 2) {
        sscanf(&block[k], "%2hhx", &bblk->WFData[i][j]);
      }
    }
    /* unpack four waveform samples from channel 2 */
    unpack_waveform_samples(wfs, &bblk->WFData[1][0]);
    for (i = 0; i < 4; ++i) {
      if (wfs[i] > 1000) wfs[i] /= 2; /* normalize spikes */
      fprintf(fp_wfdata, "%hd\n", wfs[i]);
    }
    /* read numeric non-waveform parameter data */
    for (i = 0, k = 24; i < 3; ++i, k += 2) {
      sscanf(&block[k], "%2hhx", &bblk->NonWFData[i]);
    }
    /* read checksum */
    sscanf(&block[30], "%2hhx", &bblk->CSum[0]);
    sscanf(&block[32], "%2hhx", &bblk->CSum[1]);
    /* read ones' complement of sequence number */
    sscanf(&block[34], "%2hhx", &bblk->ocoSeqNum);
    ++read_blocks;
    if (DEBUG > 0) print_debug(bblk, wfs);
  }
  free(block);
  free(bblk);

  /* print waveform status info */
  printf("[INFO] %d blocks read\n", read_blocks);
  printf("[INFO] %d QRS events detected\n", qrs_events);
  printf("[INFO] %d breaths detected\n", num_breaths);
  printf("[INFO] %d warning alarms detected\n", warn_alarms);
  printf("[INFO] %d crisis alarms detected\n", cris_alarms);

  /* check for read or write errors */
  if (ferror(fp_in)) {
    perror("fscanf(fp_in)");
    return -1;
  }
  if (fclose(fp_in) == EOF) {
    perror("fclose(fp_in)");
    return -1;
  }
  if (ferror(fp_wfdata)) {
    perror("fprintf(fp_wfdata)");
    return -1;
  }

  /* create and configure waveform plotter */
  pl_setplparam(plotter_params, "BG_COLOR", "black");
  pl_setplparam(plotter_params, "BITMAPSIZE", "1920x1080");
  pl_setplparam(plotter_params, "VANISH_ON_DELETE", "yes");
  if ((plotter = pl_newpl_r("X", NULL, NULL, stderr, plotter_params)) == NULL) {
    fprintf(stderr, "[ERROR] failed to create plotter\n");
    return -1;
  }    
  if (pl_openpl_r(plotter) < 0) {
    fprintf(stderr, "[ERROR] failed to open plotter\n");
    return -1;
  }
  pl_fspace_r(plotter, 0.0, 300.0, read_blocks / 50, 750.0); /* 50 Hz */
  pl_move_r(plotter, 0.0, 500.0); /* center vertically */
  pl_pencolorname_r(plotter, "green");

  /* display waveform */
  rewind(fp_wfdata);
  while (fscanf(fp_wfdata, "%f", &y) != EOF) {
    pl_fcont_r(plotter, x, y);
    x += 0.005;   /* one waveform sample per 5 ms */
    usleep(5000); /* 5 ms delay (4 samples per block) simulates 50 Hz timing */
  }
  if (ferror(fp_wfdata)) {
    perror("fscanf(fp_wfdata)");
    return -1;
  }

  /* cleanup */
  if (fclose(fp_wfdata) == EOF) {
    perror("fclose(fp_wfdata)");
    return -1;
  }
  if (unlink(WF_DATA) == -1) {
    perror("unlink(WF_DATA)");
    return -1;
  }
  if (pl_closepl_r(plotter) < 0) {
    fprintf(stderr, "[ERROR] failed to close plotter\n");
    return -1;
  }
  if (pl_deletepl_r(plotter) < 0) {
    fprintf(stderr, "[ERROR] failed to delete plotter\n");
    return -1;
  }
  pl_deleteplparams(plotter_params);
  return 0;
}
