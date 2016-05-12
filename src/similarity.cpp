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


#define MAX_ARGC 3
#define MIN_ARGC 3

namespace fs=boost::filesystem;

using namespace std;


void parse_input_args(int, char**, int&,  string *);

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


int main(int argc, char* argv[])
{

	//parse input arguments
	// parse_input_args(argc, argv, user, &db_path);


	string path_of_dfas = "../dfas_of_users/";
	string prefix = "wx";

	static const int n_compared_users = 4;
	const int int_comp_user[n_compared_users] = {4, 17, 25, /*41, 62, /*85, 128, 140, 144,*/ 153};

	string target_dfa_string = "017";

	// String ID of users
	vector<string> users;
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




	//////////////////////////////////////////////////////////////
	// Reed all prefixes in folder:
	// cycle on every file in folder, extract the file name and reed the prefix

	// Prefix set
	set<string> prefixes;

	fs::path someDir("../dfas_of_users/");
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

	// !?!?!?!?! ADESSO HO LETTO TUTTI I PREFISSI NELLA CARTELLA, LI DEVI SOLO METTERE
	// IN UNA STRUTTURA TIPO SET, SENZA RIPETIZIONI, COSÌ DA AVERE UNA LISTA DEI PREFISSI DA ANALIZZARE
	// PER OGNI PREFISSO DEVI COSTRUIRE LA MAPPA CHE HO DEFINITO QUI (VD ANCHE FOGLIO)
	// NB: CONTEMPLA IL CASO IN CUI PER UN PREFISSO NON CI SONO PROPRIO TUTTI GLI UTENTI!


	///////////////////////////////////////////////////////////////
	// Prefixes -> Users -> Scores with other users
	map < string, map< string, double> >  score_matrix;




	// Read DFAs of users
	map<string, gi::dfa> dfas = read_dfa_of_users(path_of_dfas, prefix, users);

	// W-METHOD comparison
	map< string, statistical_measures> stats = compare_dfas_w_method(target_dfa_string, dfas);



	// Read paired DFAs of users
	map<string, gi::dfa> paired_dfas = read_paired_dfa_of_users(path_of_dfas, prefix, users);


	// NCD comparison
	map< string, double> ncd_results = compare_dfas_NCD(target_dfa_string, dfas, paired_dfas);


	/////////////////////////////////////////////////////
	// PRINT NCD results
	cout << endl << "NCD Results:" << endl;
	for(auto &it : ncd_results)
		cout << it.first << ": " << it.second << endl;


	// PRINT W-METHOD statistical results
	for(auto &it : stats)
	{
		cout << endl << endl << "Utente "<<it.first << endl;

		cout << "TP: "<<it.second.tp<<endl;
		cout << "TN: "<<it.second.tn<<endl;
		cout << "FP: "<<it.second.fp<<endl;
		cout << "FN: "<<it.second.fn<<endl;

		cout << "Precision: "<<it.second.precision <<endl;
		cout << "Recall: "<<it.second.recall <<endl;
		cout << "F-measure: "<<it.second.f_measure << endl;

	}


	return 0;
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

		if( !fs::exists(dfa_path) )
			continue;

		//cout << "User: "<<user_id_string << endl;
		//cout << "Reed dfa path: "<<dfa_path << endl;


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
	gi::dfa target_dfa;
	try{
		target_dfa = dfas.at(target_user_string);
	}catch(exception e){
		cout << "ERR: Impossibile to find target user's dfa" << endl;
		exit(EXIT_FAILURE);
	}


	// Num states of target dfa
	int dfa_target_size = target_dfa.get_num_states();

	//cout << "DFA target letto" << endl;



	// Generation of test sets - W-METHOD
	for(auto &it : dfas){
		if( !it.first.compare(target_user_string) )
			continue;

		string path_dot = "../dfas_of_users/"+it.first+".dot";
		it.second.print_dfa_dot_mapped_alphabet("DFA", path_dot.c_str());

		cout << "DFA " << it.first << "; dimensione: "<<it.second.get_num_states() << endl;

		// Generate test set
		test_sets[it.first] = it.second.get_w_method_test_set(dfa_target_size);
	}

	cout << "Generati i test" << endl;



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

	multiset< statistical_measures, B> ordered_statistics;



	// Calculates statical index
	for(auto &it1 : stats)
	{
		it1.second.precision	= (double) it1.second.tp / (double) (it1.second.tp + it1.second.fp);
		it1.second.recall 		= (double) it1.second.tp / (double) (it1.second.tp + it1.second.fn);
		it1.second.f_measure 	= (double) (2.0 * it1.second.precision * it1.second.recall ) / (double) (it1.second.precision + it1.second.recall);
		it1.second.specificity 	= (double) it1.second.tn / (double) (it1.second.tn + it1.second.fp);
		it1.second.bcr 			= (double) (it1.second.recall + it1.second.specificity) / (double) 2.0;

		ordered_statistics.insert(it1.second);
	}


	// Print ordered statistics
	for(auto &it : ordered_statistics)
	{
		cout << it.compared_dfa << " - " << it.f_measure << endl;
	}



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




void parse_input_args(int argc, char* argv[], int &user, string *dp){
	if(argc>MAX_ARGC || argc<MIN_ARGC){
		cerr<<MSG_WRONG_ARGC<<endl;

		exit(EXIT_FAILURE);
	}

	user = stringToint(argv[1]);
	cout << "utente: "<< user << endl;

	*dp = argv[2];

}
