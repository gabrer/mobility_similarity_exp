/*
 ============================================================================
 Name        : GI-learning.cpp
 Author      : Pietro Cottone
 Version     :
 Copyright   : You can do whatever you want with this code, if you cite me ;)
 Description : Hello World in C++,
 ============================================================================
 */

#include <ctime>
#include <iostream>
#include <string>
#include <stdlib.h>
#include "dfa.h"
#include "messages.h"
#include <boost/filesystem.hpp>
#include <boost/accumulators/accumulators.hpp>	// For Accumulators
#include <boost/accumulators/statistics.hpp>	// To calculate mean and variance for an accumulator
#include <boost/tokenizer.hpp>					// Token from string (or path)
#include <set>


//#define SAMPLE_DIR "examples" 								// training set: put your training data here
//
//#define EDSM_FILE "examples.txt" 							// training set: put your training data here
//#define EDSM_FILE_BIG "examples_big.txt" 				// training set: put your training data here
//
//#define LSTAR_FILE "lstar.txt"										// file for lstar
//#define LSTAR_FILE_BIG "lstar_big.txt" 						// file for lstar
//
//#define DOT_DIR "results"											// dot file with inferred DFA
//#define DOT_FILE_BLUESTAR "inferredDFA_bluestar.dot"
//#define DOT_FILE_EDSM "inferredDFA_edsm.dot"
//#define DOT_FILE_LSTAR "inferredDFA_lstar.dot"
////#define EDSM_RES_PATH "DFA_dot" 							// by-products of inference


#define MAX_ARGC 2
#define MIN_ARGC 2

namespace fs=boost::filesystem;

using namespace std;


int parse_input_args(int, char**);

// Conformance test measures
struct statistical_measures
{
	string target_dfa = "" ;
	string compared_dfa = "" ;

	string prefix;

	// True positive, and so on.
	int tp = -1;
	int tn = -1;
	int fp = -1;
	int fn = -1;


	double precision = -1;
	double recall = -1;
	double f_measure = -1;
	double specificity = -1;
	double bcr = -1;
};


map<string, gi::dfa> read_dfa_of_users(string path_of_dfas, string prefix, vector<string> users);
map<string, gi::dfa> read_paired_dfa_of_users(string path_of_dfas, string prefix, vector<string> users);

map< string, statistical_measures>  compare_dfas_w_method(string target_user, map <string, gi::dfa> & dfas);
map< string, double> compare_dfas_NCD(string target_user, map <string, gi::dfa> & dfas, map <string, gi::dfa> & paired_dfas);

map < string, map<string, double> > get_score_matrix_for_target_user(string target_dfa_string, vector<string> users, set<string> prefixes, string path_of_dfas );


int main(int argc, char* argv[])
{

	//parse input arguments
	//int target_dfa = parse_input_args(argc, argv);

	string path_of_dfas = "../dfas_of_users/";


	/////////////////////////////////////
	//// CROSS VALL ////////////////////
	bool cross_validation = true;
	const int cross_val_iterationes = 5;
	/////////////////////////////////////


	static const int n_compared_users = 1;
	const int int_comp_user[n_compared_users] = {3/*, 4, 17, 30,  41, 62,  68,  128,  153, 163*/}; 		// Added: 41, 62, 128
	// const int int_comp_user[n_compared_users] = {3, 4, 17, 30, 68, 153, 163}; //  --> Articolo cinesi
	//const int int_comp_user[n_compared_users] = {4, 17, 25, /*41, 62,*/ 85, /*128,*/ 140, 144, 153};

	// String ID of users
	vector<string> users;


	if(!cross_validation)
	{
		for(int i=0; i<n_compared_users; ++i)
		{
			int c_user = int_comp_user[i];

			// User string id
			string user_id_string ="";
			if(c_user <10)
				user_id_string = "00"+intTostring(c_user);
			else if(c_user>=10 && c_user<100)
				user_id_string = "0"+intTostring(c_user);
			else if(c_user>=100)
				user_id_string = intTostring(c_user);

			users.push_back(user_id_string);
		}

	} else {

		if(n_compared_users != 1){
			cout << "ERR: K fold need only one user ID!" << endl;
			exit(EXIT_FAILURE);
		}

		int c_user = int_comp_user[0];

		// User string id
		string user_id_string ="";
		if(c_user <10)
			user_id_string = "00"+intTostring(c_user);
		else if(c_user>=10 && c_user<100)
			user_id_string = "0"+intTostring(c_user);
		else if(c_user>=100)
			user_id_string = intTostring(c_user);

		for(int i=0; i<cross_val_iterationes; ++i){
			users.push_back(user_id_string);
			users.push_back(user_id_string+"A-CV"+intTostring(i));
			users.push_back(user_id_string+"B-CV"+intTostring(i));
		}
	}

	// Target user
	string target_dfa_string = "";
//	for(int i=0; i<n_compared_users; ++i)
//		if(int_comp_user[i] == target_dfa)
//			target_dfa_string  = users.at(i);
//
//	if(!target_dfa_string.compare("")){
//		cerr << "ERR: Target user is not valid!" << endl;
//		return 0;
//	}
//	cout << "Target user: "<<target_dfa_string << endl;



	//////////////////////////////////////////////////////////////
	// Reed all prefixes in folder:
	// cycle on every file in folder, extract the file name and reed the prefix

	// Prefix set
	set<string> prefixes;

	fs::path someDir(path_of_dfas);
	fs::directory_iterator end_iter;

	if ( fs::exists(someDir) && fs::is_directory(someDir))
	{
	  for( fs::directory_iterator dir_iter(someDir) ; dir_iter != end_iter ; ++dir_iter)
	  {
	    if (fs::is_regular_file(dir_iter->status()) && dir_iter->path().extension() == ".txt")
	    {
	    	//cout << "FIle letto: "<< *dir_iter << endl;

	    	// Extract file name
	    	string current_filename = dir_iter->path().filename().string();


	    	// Tokenizer: check the prefix from file name
	    	boost::char_separator<char> sep("-/");
	    	boost::tokenizer<boost::char_separator<char>> tokens(current_filename, sep);
	    	string reed_prefix = (*tokens.begin());


	    	// Add reed prefix
	    	prefixes.insert(reed_prefix);


	    	//for (const auto& t : tokens)
	    	//	cout << t << "." << endl;
	    }
	  }
	}


	cout << "-- Prefissi unici --"<<endl;
	for(auto &it : prefixes)
		cout << "P: "<<it << endl;
	//////////////////////////////////////////////////////////////




	///////////////////////////////////////////////////////////////
	// Prefixes -> Users -> Scores with other users
	typedef map < string, map<string, double>> score_maps;

	map<string, score_maps> all_matrices;


	// Cycle over all the target users
	for(int i=0; i<users.size(); ++i)
	{
		target_dfa_string  = users.at(i);
		cout << "Current target user: "<<target_dfa_string << endl;

		map < string, map<string, double> >  score_matrix = get_score_matrix_for_target_user(target_dfa_string, users, prefixes, path_of_dfas);

		all_matrices[target_dfa_string] = score_matrix;
		cout << "Added matrix for target user: "<< target_dfa_string << endl;
	}


	// Print all matrices of scores
	for(auto &matrix : all_matrices)
	{
		cout <<  endl << endl << "USER: " <<matrix.first << endl;
		for(auto &prefix : matrix.second)
		{
			cout << endl <<  "Score - Prefisso: "<<prefix.first << endl;

			for(auto &it : prefix.second)
			{
				string user = it.first;
				double score = it.second;

				cout << user << " - " << score << endl;
			}
		}
	}


	// Compute an AVERAGE value of scores
	// Cycle over all the target users
	for(auto &matrix_target : all_matrices)
	{
		string target_user = matrix_target.first;

		for(auto &matrix_compared : all_matrices)
		{
			string compared_user = matrix_compared.first;

			// For all the OTHER users: read value for the same prefix and compute an average value
			if(matrix_compared.first.compare(matrix_target.first))				// Check matrices of other users
			{

				// Cycle over prefixes in the matrices
				for(auto &pair_prefix_target : matrix_target.second)			// Target 	user's matrix
				{
					// Extract the prefix in the target
					string prefix_target 	= pair_prefix_target.first;


					// Check whether there is the prefix in compared user
					if(all_matrices.at(compared_user).count(prefix_target) != 0)
					{

						// Check whether there are scores for the target and compared user respect to current prefix
						// (e.g. if w-method is too expensive it might be stopped in some case)
						if( all_matrices.at(target_user).at(prefix_target).count(compared_user) != 0 )
						{
							double target_score	  = all_matrices.at(target_user).at(prefix_target).at(compared_user);

							double average = target_score;

							if(all_matrices.at(compared_user).at(prefix_target).count(target_user) != 0){
								double compared_score = all_matrices.at(compared_user).at(prefix_target).at(target_user);
								average = ((double)target_score + (double)compared_score) / (double) 2.0;


								if(std::isnan(target_score)){
									if(!std::isnan(compared_score))
										average = compared_score;
									else
										average = NAN;
								}else if(std::isnan(compared_score))
									average = target_score;
							}


							if(std::isnan(target_score))
								average = NAN;


							all_matrices[target_user][prefix_target][compared_user] = average;
							all_matrices[compared_user][prefix_target][target_user] = average;
						}
					}
				}
			}

		}
	}



	// Print all matrices AFTER average computation
	cout << "*********************************" << endl;
	cout << "NEW MATRIX SCORES - AVERAGE" << endl;
	cout << "*********************************" << endl;
	for(auto &matrix : all_matrices)
	{
		cout << endl << endl << "USER: " <<matrix.first << endl;
		for(auto &prefix : matrix.second)
		{
			cout << endl <<  "Score - Prefisso: "<<prefix.first << endl;

			for(auto &it : prefix.second)
			{
				string user = it.first;
				double score = it.second;

				cout << user << " - " << score << endl;
			}
		}
	}



	// Print GNUPLOT format
	// Prefixes -> Users -> Scores with other users
	cout << "*********************************" << endl;
	cout << " GNUPLOT AVERAGE " << endl;
	cout << "*********************************" << endl;
	// For every prefix
	for( auto &prefix : prefixes)
	{
		cout << endl << endl << "***************" << endl;
		cout << prefix << endl;
		cout << "***************" << endl;

		// First line
		cout << "Users";
		for(auto &matrix : all_matrices)
			cout << "\t" << matrix.first;
		//cout << endl;


		// Futher lines
		for(auto &matrix : all_matrices)
		{
			// Username
			cout << endl << matrix.first;

			// Verifica che sia presente il prefisso
			if(matrix.second.count(prefix) != 0)
			{
				for(auto &compared_user : users)
				{
					if(!compared_user.compare(matrix.first)){
						cout << "\t1";
						continue;
					}

					// Whether the compared user have a value for selected prefix, print score
					if(matrix.second.at(prefix).count(compared_user) != 0)
					{
						cout.precision(3);
						if(matrix.second.at(prefix).at(compared_user) > 1)
							cout << "\t1";
						else
							cout << "\t" << matrix.second.at(prefix).at(compared_user);
					} else {
						cout << "\tna";
					}
				}

			} else {
				cout << "\tnaprefix";
			}
		}
	}



//	// Print HEATMAP format
//	// Prefixes -> Users -> Scores with other users
//	cout << "*********************************" << endl;
//	cout << " HEATMAP " << endl;
//	cout << "*********************************" << endl;
//	// For every prefix
//	for( auto &prefix : prefixes)
//	{
//		//cout << endl << endl << "***************" << endl;
//		//cout << prefix << endl;
//		//cout << "***************" << endl;
//
//
//		// Futher lines
//		for(auto &matrix : all_matrices)
//		{
//			// Username
//			//cout << endl << matrix.first;
//
//			// Verifica che sia presente il prefisso
//			if(matrix.second.count(prefix) != 0)
//			{
//				// Cerca i sovra-prefissi
//				for( auto &suppref : prefixes)
//				{
//					if (suppref.find(prefix,0) != std::string::npos) {
//					    std::cout << "found!" << '\n';
//					    cout << "Prefix: "<<prefix << "; Supprefix: "<<suppref << endl;
//					}
//				}
//			}
//		}
//	}



	// Read paired DFAs of users
//	map<string, gi::dfa> paired_dfas = read_paired_dfa_of_users(path_of_dfas, prefix, users);
//
//
//	// NCD comparison
//	map< string, double> ncd_results = compare_dfas_NCD(target_dfa_string, dfas, paired_dfas);
//
//
//	/////////////////////////////////////////////////////
//	// PRINT NCD results
//	cout << endl << "NCD Results:" << endl;
//	for(auto &it : ncd_results)
//		cout << it.first << ": " << it.second << endl;


//	// PRINT W-METHOD statistical results
//	for(auto &it : stats)
//	{
//		cout << endl << endl << "Utente "<<it.first << endl;
//
//		cout << "TP: "<<it.second.tp<<endl;
//		cout << "TN: "<<it.second.tn<<endl;
//		cout << "FP: "<<it.second.fp<<endl;
//		cout << "FN: "<<it.second.fn<<endl;
//
//		cout << "Precision: "<<it.second.precision <<endl;
//		cout << "Recall: "<<it.second.recall <<endl;
//		cout << "F-measure: "<<it.second.f_measure << endl;
//		cout << "BCR: "<<it.second.bcr << endl;
//
//		// POSSIAMO DIRE DI ESCLUDERE BCR E PRENDERE F-MEASURE
//		// PERCHE E' MOLTO PROBABILE CHE PER UN UTENTE, TRA I SAMPLE NEGATIVI, CI SIANO
//		// QUELLI DEL TARGET, FACENDO CRESCERE IL BCR.
//
//	}


	return 0;
}




map < string, map<string, double> > get_score_matrix_for_target_user(string target_dfa_string, vector<string> users, set<string> prefixes, string path_of_dfas )
{
	// Prefixes -> Users -> Scores with other users
	map < string, map<string, double> >  score_matrix;


	for(auto &prefix : prefixes)
	{

		//if(prefix.compare("wx4erj"))
		//	continue;

		cout << endl << endl << "*******************" << endl;
		cout << "Prefisso: "<<prefix << endl;
		cout << "*******************" << endl;


		// Read DFAs of users
		map<string, gi::dfa> dfas = read_dfa_of_users(path_of_dfas, prefix, users);


		// W-METHOD comparison
		try
		{
			map< string, statistical_measures> stats = compare_dfas_w_method(target_dfa_string, dfas);

			// Insert values in score matrix
			for(auto &it : stats){
				string user = it.first;
				score_matrix[prefix][user] = stats.at(user).f_measure;
			}

		}catch(const char* msg){
			string nodfa = "NO_TG_DFA";
			if(!nodfa.compare(msg))
				cerr << "ERR: Impossibile to find target user's dfa" << endl;
		}
	}


	return score_matrix;
}




// Crea una mappa con l'ID dell'utente e la relativa DFA letta
map<string, gi::dfa> read_dfa_of_users(string path_of_dfas, string prefix, vector<string> users)
{
	// Finaly reed dfas of users
	map <string, gi::dfa>  dfas;


	// It reeds from file the dfas of users
	for(auto &user_id_string : users)
	{
		string dfa_path = path_of_dfas + prefix + "-" + user_id_string +"-TXTbluestarALF.txt";


		cout << "User: "<<user_id_string << endl;
		cout << "Reed dfa path: "<<dfa_path << endl;

		if( !fs::exists(dfa_path) )
			continue;



		gi::dfa* dfa_tmp = new gi::dfa(gi::dfa::read_dfa_file(dfa_path));


		dfas[user_id_string] = *dfa_tmp;

	}

	return dfas;
}




map<string, gi::dfa> read_paired_dfa_of_users(string path_of_dfas, string prefix, vector<string> users)
{
	// Finaly reed dfas of users
	map <string, gi::dfa>  paired_dfas;

	string dfa_path = "";
	string pair_of_user_id = "";


	// It reeds from file the dfas of users
	for(auto user_id_string = users.begin(); user_id_string != users.end(); ++user_id_string)
	{
		//cout << "User: "<< (*c_user) << endl;


		for(auto it = std::next(user_id_string); it != users.end(); ++it)
		{
			//cout << "Reed user: "<< (*it) << endl;

			string paired_user = (*it);

			if(stringToint(paired_user) >  stringToint((*user_id_string)))
			{
				dfa_path = path_of_dfas + prefix + "-TXTbluestarALF-PAIR-" + (*user_id_string) +"-"+paired_user+ "-TXTbluestarALF.txt";
				pair_of_user_id = (*user_id_string) +"-"+paired_user;
			}else{
				dfa_path = path_of_dfas + prefix + "-DOTbluestarALF-PAIR-" + paired_user +"-"+(*user_id_string)+ "-TXTbluestarALF.txt";
				pair_of_user_id = paired_user +"-"+(*user_id_string);
			}


			// Reed DFA
			//cout << "Reed dfa path: "<<dfa_path << endl;
			//cout << "Coppia: "<<pair_of_user_id << endl;

			if( !fs::exists(dfa_path) )
				continue;

			gi::dfa* dfa_tmp = new gi::dfa(gi::dfa::read_dfa_file(dfa_path));

			paired_dfas[pair_of_user_id] = *dfa_tmp;
		}
	}

	return paired_dfas;
}



map< string, statistical_measures>  compare_dfas_w_method(string target_user_string, map <string, gi::dfa> & dfas)
{

	// Map associating to each user its test set (with a fixed and common target dfa)
	map< string, vector<vector<SYMBOL>> > test_sets;


	// Statistical measures of users with regard to target user
	map< string, statistical_measures> stats;



	cout << "Target user: "<<target_user_string<<endl;

	// Target dfa
	//string target_user_string = intTostring(target_user);
	if(dfas.count(target_user_string) == 0){
		//cout << "ERR: Impossibile to find target user's dfa" << endl;
		throw "NO_TG_DFA";
	}


	//Load target dfa
	gi::dfa target_dfa = dfas.at(target_user_string);

	// Num states of target dfa
	int dfa_target_size = target_dfa.get_num_states();

	//cout << "DFA target letto" << endl;



	// Generation of test sets - W-METHOD
	for(auto &it : dfas){
		if( !it.first.compare(target_user_string) )
			continue;

		//Print dot
		string path_dot = "../dfas_of_users/"+it.first+".dot";
		it.second.print_dfa_dot_mapped_alphabet("DFA", path_dot.c_str());

		cout << "---> DFA " << it.first << "; dimensione: "<<it.second.get_num_states() << endl;

		if(it.second.get_num_states() >= 15)
			throw "DFA_SIZE_TOO_BIG";

		// Generate test set
		try
		{
			test_sets[it.first] = it.second.get_w_method_test_set(dfa_target_size);
		} catch(const char* msg) {
			string err = "TEST_SET_TOO_BIG";
			if(!err.compare(msg))
				cerr << "ERR: test set too big!" << endl;
			cerr << "ERR on test generation " << endl;
		}
	}

	cout << "Generation of test sets for DFAs finished." << endl;



	// Calculates the sparse matrix (tp, tn, and so on).
	for(auto &it1 : test_sets)
	{
		string curr_user = it1.first;

		stats[curr_user].compared_dfa = curr_user;
		stats[curr_user].target_dfa = target_user_string;

		// Checks every string in test set (it1.second)
		for(auto &it2 : it1.second)
		{
			if(dfas.at(curr_user).membership_query(it2))
			{
				if(!target_dfa.membership_query(it2))
					++stats[curr_user].fp;
				else
					++stats[curr_user].tp;
			}
			else
			{
				if(target_dfa.membership_query(it2))
					++stats[curr_user].fn;
				else
					++stats[curr_user].tn;
			}
		}
	}


	// It sets the parameter compared to get ordered data.
	struct B
	{
	  bool operator()(const statistical_measures& lhs, const statistical_measures& rhs) {
	    return lhs.f_measure < rhs.f_measure;
	  }
	};

	//multiset< statistical_measures, B> ordered_statistics;



	// Calculates statical index
	for(auto &it1 : stats)
	{
		it1.second.precision	= (double) it1.second.tp / (double) (it1.second.tp + it1.second.fp);
		it1.second.recall 		= (double) it1.second.tp / (double) (it1.second.tp + it1.second.fn);
		it1.second.f_measure 	= (double) (2.0 * it1.second.precision * it1.second.recall ) / (double) (it1.second.precision + it1.second.recall);
		it1.second.specificity 	= (double) it1.second.tn / (double) (it1.second.tn + it1.second.fp);
		it1.second.bcr 			= (double) (it1.second.recall + it1.second.specificity) / (double) 2.0;

		//ordered_statistics.insert(it1.second);
	}


	// Print ordered statistics
//	for(auto &it : ordered_statistics)
//	{
//		cout << it.compared_dfa << " - " << it.f_measure << endl;
//	}



	return stats;
}



// FAI QUESTA FUNZIONE CHE RITORNA UNA MAPPA A CUI PER UN ID ASSOCIA IL VALORE DI NCD
// CONSIDERA COME SISTEMARE IL CASO DEL DFA COPPIA (anche se forse per come è impostata questa nuovo fuzione, con il nuovo prototipo, non c'è problema)
// PS: il problema della gestione dei prfeissi esistenti è esterno a questa funzione! Puoi prendere dal vecchio file di test pe rla tesi
// 		il controllo tramite libreria Boost che in file esista o meno ed evenutalmente saltarlo.
map<string, double> compare_dfas_NCD(string target_user_string, map <string, gi::dfa> & dfas, map <string, gi::dfa> & paired_dfas)
{
	double complexity_target = 0, complexity2 = 0, complexity_together = 0;

	// ID string for paired DFA
	string compared_dfa_string = "";
	string paired_dfa_string = "";

	// Calcolo media e varianza del NCD
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > accNCD;
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > accSTATI;

	// NCD values for target dfa
	map<string, double>  ncd_values;


	// Check if paired_dfas is empty
	if(paired_dfas.size() == 0)
	{
		cout << "No pairs to compute" << endl;
		exit(EXIT_FAILURE);
	}


	// Target dfa
	//string target_user_string = intTostring(target_user);
	gi::dfa target_dfa;
	try{
		target_dfa = dfas.at(target_user_string);
	}catch(exception e){
		cout << "ERR: Impossibile to find target user's dfa" << endl;
		exit(EXIT_FAILURE);
	}

	// Get dfa target complexity
	complexity_target = target_dfa.get_num_states();



	// Compute NCD
	for(auto &it : dfas)
	{

		if( !it.first.compare(target_user_string) )
			continue;

		// ID of compared DFA
		compared_dfa_string = it.first;


		// ID of paired DFA
		if(stringToint(target_user_string) < stringToint(compared_dfa_string))
			paired_dfa_string = target_user_string + "-" + compared_dfa_string;
		else
			paired_dfa_string = it.first + "-" + target_user_string;

		// Check if exists, else go to next dfa
		if( paired_dfas.count(paired_dfa_string) == 0 )
			continue;


		// Complexity
		complexity2 = it.second.get_num_states();

		complexity_together = paired_dfas.at(paired_dfa_string).get_num_states();


		// Compute NCD
		double curr_NCD = /*fabs(*/ ncd(complexity_target, complexity2, complexity_together ) /*)*/;

		//cout << "Aggiunto: "<<compared_dfa_string << ", value: "<<curr_NCD << endl;
		//cout << "DFA target: "<<complexity_target << ", DFA "<<compared_dfa_string << ": "<<complexity2 << "; DFA Paired: "<<complexity_together << endl;
		ncd_values[compared_dfa_string] = curr_NCD;


		// Record stats
		accNCD(curr_NCD);

		accSTATI(complexity_target);

		accSTATI(complexity2);

		accSTATI(complexity_together);
	}



	cout << endl << "Media NCD: "<< boost::accumulators::mean(accNCD) << endl << "Dev Std NCD: "<< sqrt(boost::accumulators::variance(accNCD)) << endl;
	cout << "Dev Std norm NCD: "<< sqrt(boost::accumulators::variance(accNCD))/boost::accumulators::mean(accNCD) << endl;

	cout << endl << "Media n stati: "<< boost::accumulators::mean(accSTATI) << endl << "Dev Std n stati: "<< sqrt(boost::accumulators::variance(accSTATI)) << endl;
	cout << "Dev Std norm n stati: "<< sqrt(boost::accumulators::variance(accSTATI))/boost::accumulators::mean(accSTATI)<< endl;


	return ncd_values;

}





int parse_input_args(int argc, char* argv[]){
	if(argc>MAX_ARGC || argc<MIN_ARGC){
		cerr<<MSG_WRONG_ARGC<<endl;

		exit(EXIT_FAILURE);
	}

	int user = stringToint(argv[1]);
	cout << "Reed target user: "<< user << endl;

	return user;
	//*dp = argv[2];

}
