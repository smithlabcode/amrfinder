/* 
 *    Copyright (C) 2009 University of Southern California and
 *                       Andrew D. Smith
 *                       Song Qiang
 *
 *    Authors: Song Qiang
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// #define NDEBUG

#include <string>
#include <vector>
#include <iostream>
#include <iterator>
#include <fstream>
#include <cmath>

#include "smithlab_os.hpp"
#include "OptionParser.hpp"
#include "smithlab_utils.hpp"
#include "GenomicRegion.hpp"


using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream_iterator;
using std::max;
using std::min;
using std::numeric_limits;
using std::pair;
using std::make_pair;

inline bool static 
is_pairend(const size_t flag) {return flag & 0x1;}

inline bool static 
is_singlend(const size_t flag) {return !is_pairend(flag);}

inline bool static
is_Trich(const size_t flag) {return flag & 0x40;}

inline bool static
is_Arich(const size_t flag) {return flag & 0x80;}

inline bool static
is_mapping_paired(const size_t flag) {return flag & 0x2;}

inline bool static 
is_unmapped(const size_t flag) {return flag & 0x8;}

inline bool static 
is_mapped(const size_t flag) {return !(is_unmapped(flag));}

inline bool static
is_revcomp(const size_t flag) {return flag & 0x10;}

void static
apply_CIGAR(const string &seq, const string &qual,
            const string &CIGAR, string &new_seq, string &new_qual)
{
    assert(seq.size() == qual.size());
    assert(new_seq.size() == 0 && new_qual.size() == 0);
    size_t n;
    char op;
    size_t i = 0;

    std::istringstream iss(CIGAR);
    while (iss >> n >> op)
    {
        switch (op)
        {
        case 'M':
            new_seq += seq.substr(i, n);
            new_qual += qual.substr(i, n);
            i += n;
            break;
        case 'I':
            i += n;
            break;
        case 'D':
            new_seq += string(n, 'N');
            new_qual += string(n, 'B');
            break;
        case 'S':
            i += n;
            break;
        case 'H':
            ;
            break;
        case 'P':
            ;
            break;
        case '=':
            ;
            break;
        case 'X':
            ;
            break;
        }
    }
    
    assert(i == seq.length());
    assert(new_seq.size() == new_qual.size());
}

inline static void
get_mismatch(const string &mismatch_str, size_t &mismatch)
{
    mismatch = atoi(mismatch_str.substr(5).c_str());
}

inline static void
get_strand(const string &strand_str, string &strand, string &bs_forward)
{
    strand = strand_str.substr(5, 1);
    bs_forward = strand_str.substr(6, 1);
    if (bs_forward == "-") strand = strand == "+" ? "-" : "+";
}


int 
main(int argc, const char **argv) 
{
    try 
    {
        string infile("/dev/stdin");
        string Toutfile;
        string Aoutfile("/dev/null");

        bool VERBOSE;
        
        /****************** COMMAND LINE OPTIONS ********************/
        OptionParser opt_parse(strip_path(argv[0]),
                               "convert SAM output of BSMAP to MR format of RMAP"
                               "");
        opt_parse.add_opt("T-rich", 'T', "Trich reads Output file", 
                          OptionParser::REQUIRED, Toutfile);
        opt_parse.add_opt("A-rich", 'A',
                          "Arich reads Output file (ignore for SE mapping)", 
                          OptionParser::OPTIONAL, Aoutfile);
        opt_parse.add_opt("verbose", 'v', "print more run info", false, VERBOSE);

        vector<string> leftover_args;
        opt_parse.parse(argc, argv, leftover_args);
        if (argc == 1 || opt_parse.help_requested()) 
        {
            cerr << opt_parse.help_message() << endl;
            return EXIT_SUCCESS;
        }
        if (opt_parse.about_requested()) 
        {
            cerr << opt_parse.about_message() << endl;
            return EXIT_SUCCESS;
        }
        if (opt_parse.option_missing()) 
        {
            cerr << opt_parse.option_missing_message() << endl;
            return EXIT_SUCCESS;
        }
        if (leftover_args.size() > 0) infile = leftover_args.front();
        /****************** END COMMAND LINE OPTIONS *****************/

        std::ifstream in(infile.c_str());
        std::ofstream tout(Toutfile.c_str());
        std::ofstream aout(Aoutfile.c_str());

        
        string line;
        size_t nline = 1;
        while (std::getline(in, line))
        {
            string name, chrom, CIGAR, mate_name, seq,
                qual, mismatch_str, strand_str;

            size_t flag, start, mapq_score, mate_start;
            int seg_len;
            std::istringstream iss(line);
            if (iss >> name >> flag >> chrom >> start >> mapq_score >> CIGAR
                >> mate_name >> mate_start >> seg_len >> seq >> qual
                >> mismatch_str >> strand_str)
            {
                if (is_pairend(flag))
                {
                    assert(Aoutfile != "/dev/null" && aout.good());
                    assert(!(flag & 0x4)); // only working with mapped reads
                    --start; // SAM are 1-based

                    size_t mismatch;
                    get_mismatch(mismatch_str, mismatch);
                
                    string strand, bs_forward;
                    get_strand(strand_str, strand, bs_forward);

                    assert(is_Trich(flag) && bs_forward == "+"
                           || is_Arich(flag) && bs_forward == "-");

                    string new_seq, new_qual;
                    apply_CIGAR(seq, qual, CIGAR, new_seq, new_qual);

                    if (is_revcomp(flag))
                    {
                        revcomp_inplace(new_seq);
                        std::reverse(new_qual.begin(), new_qual.end());
                    }
                
                    if (is_Trich(flag))
                    {
                        tout << chrom << "\t" << start << "\t"
                             << start + new_seq.length() << "\t"
                             << (name + "/1") << "\t" << mismatch << "\t"
                             << strand << "\t" << new_seq << "\t"
                             << new_qual << "\t" << endl;
                    }
                    else
                    {
                        aout << chrom << "\t" << start << "\t"
                             << start + new_seq.length() << "\t"
                             << (name + "/2") << "\t" << mismatch << "\t"
                             << strand << "\t" << new_seq << "\t"
                             << new_qual << "\t" << endl;
                    }
                }
                else 
                {
                    assert(!(flag & 0x4)); // only working with mapped reads
                    --start; // SAM are 1-based

                    size_t mismatch;
                    get_mismatch(mismatch_str, mismatch);
                
                    string strand, bs_forward;
                    get_strand(strand_str, strand, bs_forward);

                    assert(bs_forward == "+");
                    assert(is_revcomp(flag) == (strand != bs_forward));
                
                    if (is_revcomp(flag))
                    {
                        revcomp_inplace(seq);
                        std::reverse(qual.begin(), qual.end());
                    }
                 
                    string new_seq, new_qual;
                    apply_CIGAR(seq, qual, CIGAR, new_seq, new_qual);
                
                    tout << chrom << "\t" << start << "\t"
                         << start + new_seq.length() << "\t"
                         << name << "\t" << mismatch << "\t"
                         << strand << "\t" << new_seq << "\t"
                         << new_qual << "\t" << endl;
                }
            }
            else
            {
                cerr << "Line " << nline << ": " << line << endl;
            }
        }

        in.close();
        tout.close();
        aout.close();
    }
    catch (const SMITHLABException &e) 
    {
        cerr << "ERROR:\t" << e.what() << endl;
        return EXIT_FAILURE;
    }
    catch (std::bad_alloc &ba) 
    {
        cerr << "ERROR: could not allocate memory" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
