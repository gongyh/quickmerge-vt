// This program takes two different assemblies in fasta format and uses the
// delta output from mummer to generate a merged asembly The program does not
// correct any assembly errors. It only joins contigs. So if a contig was
// misassembled  by the main assembly program, it will most likely remain
// misassembled. Currently, the program is intended to use when 30X or above
// pacbio data is available, such that at least a respectable self-assembly is
// available. Please send questions and bug reports to mchakrab@uci.edu

#include "qmerge.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  if ((argc < 14) || (string(argv[1]) == "-h") ||
      (string(argv[1]) == "--help")) {
    cout << endl;
    cerr << "Usage: " << argv[0]
         << " -d out.delta -q query.fasta -r reference.fasta -hco "
            "(default=5.0) -c (default=1.5) -l seed_length_cutoff -ml "
            "merging_length_cutoff -p prefix"
         << endl;
    cout << "=========================================================" << endl;
    cout << "quickmerge version 0.3" << endl;
    cout << "   Options:\n";
    cout << "       -d : delta alignment file from nucmer\n";
    cout << "       -q : fasta used as query in nucmer\n";
    cout << "       -r : fasta used as reference in nucmer\n";
    cout << "     -hco : seed alignment HCO cutoff (default=5.0)\n";
    cout << "       -c : high confidence overlap cutoff (default=1.5)\n";
    cout << "       -l : seed alignment length cutoff (long integer)\n";
    cout << "      -ml : merging length cutoff (integer)\n";
    cout << "       -p : output prefix\n";
    cout << "-h/--help : prints this help\n";
    exit(EXIT_FAILURE);
  }
  if (string(argv[1]) == "-v") {
    cout << "quickmerge version 0.4" << endl;
    exit(EXIT_FAILURE);
  }
  ifstream fin, hyb, pb;
  ofstream fout;

  asmMerge merge, merge1;
  fastaSeq hybrid, pbOnly, merged;

  string header, ref_name, qu_name, tempname, name, last_tempname;
  vector<string> vs;
  vs = argParser(argv);
  string mergeName = "merged_" + vs[7] + ".fasta";
  fout.open(mergeName);
  double hco, cutoff;
  hco = 5.0;
  cutoff = 1.5;
  int qu_st = 0;
  int qu_end = 0;
  int r_st = 0;
  int r_end = 0;
  const int length = stoi(vs[5], NULL);
  const int absLenCutoff = stoi(vs[6], NULL);
  if (vs[3] != "") {
    hco = stod(vs[3], NULL);
  }
  if (vs[4] != "") {
    cutoff = stod(vs[4], NULL);
  }
  fin.open(vs[0]);

  while (getline(fin, header)) {
    if (header[0] == '>') {
      // If we've switched to a new tempname (and it's not the first iteration)
      if (!tempname.empty() && tempname != last_tempname) {
        last_tempname = tempname;
        // --- Check mixed strands for last_tempname ---
        bool has_forward = false, has_reverse = false;
        for (size_t i = 0; i < merge.q_st[last_tempname].size(); ++i) {
          if (merge.q_st[last_tempname][i] < merge.q_end[last_tempname][i])
            has_forward = true;
          else if (merge.q_st[last_tempname][i] > merge.q_end[last_tempname][i])
            has_reverse = true;
          if (has_forward && has_reverse)
            break;
        }

        if (has_forward && has_reverse) {
          // Find the longest MUM
          size_t longest_idx = 0;
          int max_len = 0;
          for (size_t i = 0; i < merge.q_st[last_tempname].size(); ++i) {
            int len = std::abs(merge.q_st[last_tempname][i] -
                               merge.q_end[last_tempname][i]);
            if (len > max_len) {
              max_len = len;
              longest_idx = i;
            }
          }

          // Keep only the longest MUM
          int keep_ref_st = merge.ref_st[last_tempname][longest_idx];
          int keep_ref_end = merge.ref_end[last_tempname][longest_idx];
          int keep_q_st = merge.q_st[last_tempname][longest_idx];
          int keep_q_end = merge.q_end[last_tempname][longest_idx];

          merge.ref_st[last_tempname].clear();
          merge.ref_end[last_tempname].clear();
          merge.q_st[last_tempname].clear();
          merge.q_end[last_tempname].clear();

          merge.ref_st[last_tempname].push_back(keep_ref_st);
          merge.ref_end[last_tempname].push_back(keep_ref_end);
          merge.q_st[last_tempname].push_back(keep_q_st);
          merge.q_end[last_tempname].push_back(keep_q_end);
        }
      }

      ref_name = xtractcol(header, ' ', 1);
      ref_name = ref_name.substr(1);
      merge.r_name.push_back(ref_name);
      qu_name = xtractcol(header, ' ', 2);
      merge.q_name.push_back(qu_name);
      tempname = ref_name.append(
          qu_name); // tempname is the index for the map. they describe
                    // alignment pairs( e.g. BackboneX ctgY). should be unique.
      merge.ref_len[tempname] = atoi(xtractcol(header, ' ', 3).c_str());
      merge.q_len[tempname] = atoi(xtractcol(header, ' ', 4).c_str());
    }

    if (header[0] != '>' && header.size() > 10) {
      r_st = atoi(xtractcol(header, ' ', 1).c_str());
      merge.ref_st[tempname].push_back(
          r_st); // storing the coordinates for each alignment hit
      r_end = atoi(xtractcol(header, ' ', 2).c_str());
      merge.ref_end[tempname].push_back(r_end);
      qu_st = atoi(xtractcol(header, ' ', 3).c_str());
      merge.q_st[tempname].push_back(qu_st);
      qu_end = atoi(xtractcol(header, ' ', 4).c_str());
      merge.q_end[tempname].push_back(qu_end);
    }
  }

  // check the last tempname after the loop ends!
  last_tempname = tempname;
  if (!last_tempname.empty()) {
    bool has_forward = false, has_reverse = false;
    for (size_t i = 0; i < merge.q_st[last_tempname].size(); ++i) {
      if (merge.q_st[last_tempname][i] < merge.q_end[last_tempname][i])
        has_forward = true;
      else if (merge.q_st[last_tempname][i] > merge.q_end[last_tempname][i])
        has_reverse = true;
      if (has_forward && has_reverse)
        break;
    }
    if (has_forward && has_reverse) {
      size_t longest_idx = 0;
      int max_len = 0;
      for (size_t i = 0; i < merge.q_st[last_tempname].size(); ++i) {
        int len = std::abs(merge.q_st[last_tempname][i] -
                           merge.q_end[last_tempname][i]);
        if (len > max_len) {
          max_len = len;
          longest_idx = i;
        }
      }
      int keep_ref_st = merge.ref_st[last_tempname][longest_idx];
      int keep_ref_end = merge.ref_end[last_tempname][longest_idx];
      int keep_q_st = merge.q_st[last_tempname][longest_idx];
      int keep_q_end = merge.q_end[last_tempname][longest_idx];

      merge.ref_st[last_tempname].clear();
      merge.ref_end[last_tempname].clear();
      merge.q_st[last_tempname].clear();
      merge.q_end[last_tempname].clear();

      merge.ref_st[last_tempname].push_back(keep_ref_st);
      merge.ref_end[last_tempname].push_back(keep_ref_end);
      merge.q_st[last_tempname].push_back(keep_q_st);
      merge.q_end[last_tempname].push_back(keep_q_end);
    }
  }

  fin.close();

  writeToFile(merge, vs[7]);
  ovlStoreCalculator(merge);
  innieChecker(merge);
  sideChecker(merge);
  sideCheckerR(merge);
  sideCheckerQ(merge);
  assignStrand(merge);
  // ovlStoreCalculator(merge);
  nOvlStoreCalculator(merge);
  ovrHngCal(merge);
  overHangSideR(merge);
  writeSummary(merge, vs[7]);

  if (vs[1] != "") {
    hyb.open(vs[1]);
  }
  if (vs[2] != "") {
    pb.open(vs[2]);
  }
  fillSeq(hybrid, hyb, ' ');
  if (string(argv[16]) == "Y") {
    splitHaplo(merge, hybrid);
  }
  fillSeq(pbOnly, pb);
  fillAnchor(merge, merge1, hco, cutoff, length, absLenCutoff, hybrid);
  writeAnchorSummary(merge, vs[7]);

  findChain(merge, merge1, pbOnly, merged, cutoff);
  createMseq(merge, merge1);

  checkAln(merge, merge1);

  fillOri(merge, merge1);

  for (map<string, vector<string>>::iterator it = merge1.lseq.begin();
       it != merge1.lseq.end(); it++) {
    name = it->first;
    cout << name << "\t";
    for (unsigned int j = 0; j < merge1.lseq[name].size(); j++) {
      cout << merge1.lseq[name][j] << "\t" << merge.Ori[name][j] << "\t";
    }
    cout << endl;
  }
  // checkAln(merge,merge1);
  removeSeq(merge, hybrid, merged); // copy unaligned hybrid contigs to merged
  // trimSeq(merge,hybrid);
  ctgJoiner(merge, merge1, hybrid, pbOnly, merged);
  writeMerged(merged, fout);
  fout.close();
  return 0;
}
