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
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>

#include "geoExp.h"

#define EXP_DIR "experiments"

#define MAX_BUFFER_SIZE 				256


#define PREFIX_NUMBER_LIMIT 		1000000				// Eventuale limite nel numero di prefissi analizzati per utente

#define MIN_NUMBER_POSITIVE_SAMPLES  	25
#define MIN_NUMBER_TEST_SAMPLES 			5

#define K_FOLD_CROSS_VAL 							5

//#include <ctime>
#include <chrono>

namespace fs=boost::filesystem;

using namespace std;


// An experiment is charatterizated by a single user
geoExp::geoExp(string db_path, int user, int min_prefixes_length, int max_prefixes_length, bool repetitions, int train_proportion, double cold_start_proportion, \
								bool alg_edsm, bool alg_blues, double alpha, double delta)
{
	this->db_path = db_path;
	this->user = user;

	if(min_prefixes_length < 0 || min_prefixes_length > 9)
		min_prefixes_length = 1;													//TODO: da decidere se mettere a 1 facendo dei test
	this->min_prefixes_length = min_prefixes_length;


	if(max_prefixes_length <0 || max_prefixes_length > 9)
		max_prefixes_length = 9;
	this->max_prefixes_length = max_prefixes_length;


	this->no_repetitions_inside_strings = repetitions;


	this->cold_start_proportion = cold_start_proportion;

	this->training_proportion = train_proportion;

	this->test_proportion = 100 - train_proportion;


	int tmp_min_n_positive_samples = MIN_NUMBER_POSITIVE_SAMPLES;
	this->cross_val_run = (double) (test_proportion * tmp_min_n_positive_samples) / (double) 100.0;
	cout << "Number of run for cross validation: "<<cross_val_run << endl;


	edsm 		= alg_edsm;
	blueStar 	= alg_blues;
	if(blueStar){
		alpha_value = alpha;
		delta_value = delta;
	}
	else if(alpha != 0.0){
		cerr << "Alpha values selected without BlueStar algorithm"<<endl;
		exit(EXIT_FAILURE);
	}




	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Check or create the root "experiments" folder
	set_root_exp_folders();


	// Create the current experiment folder
	set_current_exp_folder();
}



//TODO: verificare che venga invoca il distruttore della classe "bluefringe"
geoExp::~geoExp(){};



// Find and set the execution folder, it's the base path
void geoExp::set_root_exp_folders()
{
	// Set the main directory: is the parent directory of execution folder
	exec_path = fs::current_path().parent_path().c_str();
	exec_path = exec_path + fs::path::preferred_separator;


	// Create the experiments folder, if not exists
	// From the execution folder, check and create an "experiments" folder.
	// Inside it, there are folders for every different length of the exmperiment
	// There are ".." beacuse exectuion folder is downside the main folder
	if( !fs::exists(exec_path+EXP_DIR) )
		root_exp_path = create_folder(exec_path, EXP_DIR, false);
	else
		root_exp_path = exec_path+EXP_DIR+"/";

	cout << "\"Experiments\" folder checked" << endl;
}


// Find and set the execution folder, it's the base path
void geoExp::set_current_exp_folder()
{

	// Create the current experiment folder
	// From the execution folder, check and create an "experiments" folder.
	// Inside it, there are folders for every different length of the exmperiment
	if(edsm)
		current_exp_folder = create_folder(root_exp_path, "user"+intTostring(user)+"_EDSM", true);
	else if(blueStar)
		current_exp_folder = create_folder(root_exp_path, "user"+intTostring(user)+"_BLUES", true);

	cout << "Current experiment directory is: "<<endl << current_exp_folder<<endl;

}



// Se "current_time" è true crea comunque una nuova cartella con l'orario di invocazione
string geoExp::create_folder(const string  base_path, const string new_folder, bool current_time){

	// Path of the new folder
	string new_path;

	// Define the folder for experiment results, if exists create a folder with modified name with current time
	new_path = base_path + new_folder;


	// move res_dir if it exists
	if(fs::exists(new_path) || current_time)
	{
		char append_time[MAX_BUFFER_SIZE];
		time_t rawtime = std::time(NULL);
		struct tm * timeinfo;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		strftime(append_time, MAX_BUFFER_SIZE, "%m_%d_%H_%M_%S", timeinfo);

		// Check if the folder exist yet, differntly change only the path and don't rename an inexistent folder
		if(fs::exists(new_path))
			fs::rename(new_path, new_path + "_" + append_time);
		else
			new_path = new_path + "_" + append_time;
	}


	// create res_dir
	fs::create_directory( new_path );


	// Update the exp_path for future use
	new_path = new_path + fs::path::preferred_separator;


	return new_path;
}



void geoExp::run_inference_accuracy()
{
	cout << "Selected algorithms are: " << endl;

	if(edsm)
		cout << "EDSM" << endl;
	if(blueStar)
		cout << "BlueStar"<<endl;


	cout << "Database path: "<<db_path<<endl;


	// Utenti analizzati
	string users[160];
	for(int i=0; i<160; ++i){
		if(i <10)
			users[i] = "00"+intTostring(i);
		else if(i>=10 && i<100)
			users[i] = "0"+intTostring(i);
		else if(i>=100)
			users[i] = intTostring(i);
	}

	cout << "Lunghezza minima dei prefissi: " << min_prefixes_length << endl;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PREFIXES LENGHT
	for(int j=min_prefixes_length; j<max_prefixes_length; ++j)
	{
		string folder_current_prefix_len = "";
		set<string>::iterator it;
		int limit_prefixes_number = 0;

		cout << endl << endl << endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;
		cout <<  "---------- PREFISSI DI LUNGHEZZA "+intTostring(j)+", utente "+users[user]+" -----------"<<endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;

		clock_t tStart = clock();


		// Open connection to DB
		geodb* mydb = new geodb(db_path);
		mydb->connect();

		// Prefixes of length 'j'
		set<string>* prefixes = mydb->get_prefixes_for_user(users[user], j);

		mydb->close();
		delete mydb;

		if(prefixes->size() == 0){
			cout << "No prefixes for this user!" << endl;
			continue;
		}


		printf("Time taken to extract prefixesL:%d %.2fs\n",j, (double)(clock() - tStart)/CLOCKS_PER_SEC);


		// Make dir for all prefixes of length 'l'
		folder_current_prefix_len = create_folder(current_exp_folder, intTostring(j), false);


		// Istance the stat structure
		mystat statistics[prefixes->size()];

		for(int k=0; k<prefixes->size(); ++k)
		{
			statistics[k].percentage_positive_bluestar =  new double[cross_val_run];
			statistics[k].num_states_bluestar =  new int[cross_val_run];
			statistics[k].errore_rate_bluestar =  new double[cross_val_run];
			statistics[k].num_actual_merges_bluestar = new int[cross_val_run];
			statistics[k].num_heuristic_evaluations_bluestar =  new int[cross_val_run];

			for(int i=0; i<cross_val_run; ++i){
				statistics[k].percentage_positive_bluestar[i] =  -1;
				statistics[k].num_states_bluestar[i] =  -1;
				statistics[k].errore_rate_bluestar[i] =  -1;
				statistics[k].num_actual_merges_bluestar[i] = -1;
				statistics[k].num_heuristic_evaluations_bluestar[i] =  -1;
			}
		}


		// Cicla i PREFISSI
		cout << endl << prefixes->size() << " prefixes of length " << j <<", limit to ALL";



		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// ALL PREFIXES FOR CURRENT LENGTH
		for (it=prefixes->begin(); it!=prefixes->end() && limit_prefixes_number < PREFIX_NUMBER_LIMIT; ++it)
		{
			cout << endl << endl << endl << "***************************************************" << endl;
			cout <<  "------> Current Prefix: "<<*it << " - Utente "+users[user]<< " <--------"<<endl;
			cout << "***************************************************" << endl;
			limit_prefixes_number++;


			string*	test_set = NULL;
			geodb*	mydb = NULL;
			string	path_samples = "";
			string	path_training_data = "";
			string 	path_test_data = "";

			double succ_rate = -1;				// Success rate for current prefix



			// *********************************
			//     WRITE MINITRAJECTORIES FILE
			// *********************************

			// Open connection to DB
			mydb = new geodb(db_path);
			mydb->connect();


			// Current prefix
			string current_prefix = *it;
			statistics[limit_prefixes_number-1].prefix= current_prefix;

			clock_t tStart = clock();
			//cout << "Estraggo e scrivo le minitraiettorie..."<<endl;


			// Path per scrivere le minitraiettorie come samples, per il prefisso considerato
			path_samples			= 	folder_current_prefix_len+current_prefix;
			path_training_data	= path_samples + "-samples-CV0.txt";
			path_test_data 		= path_samples + "-test_samples-CV0.txt";

			int dim_test = 0;

			try
			{
				// Scrivi le ministraiettorie di training su file di testo e quelle di test nella variabile locale test_set (puntatore)
				//write_minitraj_from_db_like_samples(users[i], current_prefix, path_samples );
				dim_test = write_minitraj_from_db_like_samples_TRAINTEST_TESTSET(users[user], current_prefix, path_samples);
			}
			catch (const char* msg )
			{
				cout << msg << endl;
				cout << " >-------------------------------< "<< endl;

				mydb->close();
				delete mydb;

				continue;
			}


			mydb->close();
			delete mydb;


			printf("Time taken to write minitrajectories to file: %.2fs\n", (double)(clock() - tStart)/CLOCKS_PER_SEC);


			tStart = clock();





			if(edsm)
			{
				// *********************
				// 	    EDSM algorithm
				// *********************
				cout << endl << "******** EDSM "+current_prefix+"  ********" << endl;


				// Read positive and negative samples
				gi::edsm* edsm_exe = new gi::edsm(path_training_data.c_str());


				// Start inference
				gi::dfa* EDSM_dfa = edsm_exe->run(folder_current_prefix_len+current_prefix+"-");


				// Print time taken
				printf("Time taken to EDSM: %.2fs\n", (double)(clock() - tStart)/CLOCKS_PER_SEC);


				// *********************
				// 	    EDSM Statistics
				// *********************

				statistics[limit_prefixes_number-1].num_states_edsm = EDSM_dfa->get_num_states();
				statistics[limit_prefixes_number-1].num_actual_merges_edsm = edsm_exe->get_actual_merge();
				statistics[limit_prefixes_number-1].num_heuristic_evaluations_edsm = edsm_exe->get_heuristic_merge();

				// Statistiche di generalizzazione
				if(dim_test != 0)
				{
					succ_rate = make_generalization_test(EDSM_dfa, path_test_data.c_str()/*, dim_test*/);


					// Statistics
					if(dim_test == 0)
						succ_rate = 1;


					statistics[limit_prefixes_number-1].percentage_positive_edsm =  succ_rate;
					cout << "Tasso di generalizzazione di EDSM: "<<succ_rate*100<<"%"<<endl;

				}
				else
					cout << "XXX Caso banale: una sola stringa positiva XXX"<<endl;


				// *********************
				// 	    EDSM print
				// *********************

				// Print automata
				//EDSM_dfa->print_dfa_ttable("- Edsm dfa -");

				//string ttableEdsmpath = folder_current_prefix_len+"-TABLEedsm.txt";
				//EDSM_dfa.print_dfa_transition_table_file(ttableEdsmpath.c_str());

				string dotEdsmpath = folder_current_prefix_len+current_prefix+"-DOTedsm.dot";
				EDSM_dfa->print_dfa_dot("EDSM", dotEdsmpath.c_str());

				string dotEdsmpath_alf = folder_current_prefix_len+current_prefix+"-DOTedsmALF.dot";
				EDSM_dfa->print_dfa_dot_mapped_alphabet("EDSM", dotEdsmpath_alf.c_str());
				cout << "EDSM  DFA path: "<<dotEdsmpath_alf << endl;


				// Delete
				if(edsm_exe != NULL)
					delete edsm_exe;
				if(EDSM_dfa != NULL)
					delete EDSM_dfa;
			}

			tStart = clock();



			if(blueStar)
			{
					for(int i=0; i<cross_val_run; ++i)
					{
						// *********************
						// 	 BLUESTAR algorithm
						// *********************
						cout << endl<< "********  BLUESTAR "+current_prefix+"-cv: "+intTostring(i)+" *********" << endl;
						gi::dfa* BLUESTAR_dfa;

						path_training_data	= path_samples + "-samples-CV"+intTostring(i)+".txt";
						path_test_data 		= path_samples + "-test_samples-CV"+intTostring(i)+".txt";


						// Read positive and negative samples
						gi::blueStar* bluestar_exe = new gi::blueStar(path_training_data.c_str(), alpha_value, delta_value);


						// Start inference
						try
						{

							BLUESTAR_dfa = bluestar_exe->run(folder_current_prefix_len+current_prefix+"-");

						}
						catch( const char* msg ){	// NB: Questo potrebbe sfalzare la corrispondenza tra i valori nel file STATISTICS e i prefissi a cui si riferiscono
							cout << msg << "; "<<endl;
							cout << " >---------------------------------------< " << endl;
							continue;
						}

						// Print time taken
						cout<<"Time taken to BLUESTAR: " <<(double)(clock() - tStart)/CLOCKS_PER_SEC << " sec" << endl;



						// *********************
						// 	 BLUESTAR Statistics
						// *********************

						// Statistiche di generalizzazione
						if(dim_test != 0)
						{
							succ_rate = make_generalization_test(BLUESTAR_dfa, path_test_data.c_str()/*, dim_test*/);


							// Statistics
							if(dim_test == 0)
								succ_rate = 1;


							statistics[limit_prefixes_number-1].percentage_positive_bluestar[i] =  succ_rate;
							statistics[limit_prefixes_number-1].num_states_bluestar[i] = BLUESTAR_dfa->get_num_states();
							statistics[limit_prefixes_number-1].errore_rate_bluestar[i] = bluestar_exe->get_error_rate_final_dfa();
							statistics[limit_prefixes_number-1].num_actual_merges_bluestar[i] = bluestar_exe->get_actual_merge();
							statistics[limit_prefixes_number-1].num_heuristic_evaluations_bluestar[i] = bluestar_exe->get_heuristic_merge();

							cout << "Tasso di generalizzazione di BLUESTAR: "<<succ_rate*100<<"%"<<endl;
							cout << "Error-rate sulle stringhe positive: "<< bluestar_exe->get_error_rate_final_dfa() << endl;


						}
						else
							cout << "XXX Caso banale: una sola stringa positiva XXX"<<endl;





						// *********************
						// 	    BLUESTAR  print
						// *********************

						// Print transition table of the inferred automaton
						//BLUESTAR_dfa.print_dfa_ttable("- BlueStar dfa -");

						// Create dot figure for the inferred automaton
						string dotBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-DOTbluestarALF-CV"+intTostring(i)+".dot";
						BLUESTAR_dfa->print_dfa_dot_mapped_alphabet("BlueStar", dotBlueStarpath_alf.c_str());
						cout << "BLUSTAR DFA path: "<<dotBlueStarpath_alf << endl;



						// free allocated memory
						if(bluestar_exe!=NULL)
							delete bluestar_exe;
						if(BLUESTAR_dfa != NULL)
							delete BLUESTAR_dfa;

					} // CROSS VALL FOR

			}

		}


		//Statistics for a fixed legnth of prefix
		if(edsm)
			write_statistics_files(statistics, prefixes->size(), intTostring(user), true, 0);

		for(int i=0; i<cross_val_run; ++i)
			if(blueStar)
				write_statistics_files(statistics, prefixes->size(), intTostring(user), false, i);


		stat_final_results(statistics, prefixes->size(), intTostring(user), false);

		stat_final_results_minimal(statistics, prefixes->size(), intTostring(user), false);


		prefixes->clear();
		delete prefixes;

	}

}




// Estrae le statistiche di generalizzazione dell'automa
// (a differenza di quello implementato dentro edsm.cpp,
//  questo ha in input le stringhe di test non ancora prelaborate)
double geoExp::make_generalization_test(gi::dfa* finaldfa, const char * file_path/*,  int dim_test*/)
{
	cout << "**** TEST DI GENERALIZZAZIONE ****" << endl;

	int* wp;
	int dim_testset;

	int total_weigth = 0;


	// Read test set from file
	string* test_set = read_testsamples_leaving_geohash_encoding(file_path, dim_testset, wp);

	cout << "Stringhe di test lette dal file: "<<dim_testset << endl;

	int successful = 0;

	for(int i=0; i<dim_testset; ++i)
	{
		// Stringa (eventualmente) senza ripetizioni
		// (devo fare il mapping invero dell'alfabato)
		//cout << "Stringa letta: "<< test_set[i] << ", peso: "<< wp[i] << endl;
		string test_string_tmp = test_set[i];
		vector<SYMBOL> test_string;

		//string test_string_tmp = add_space_between_characters_delete_repetitions(test_set[i]);


		total_weigth += wp[i];

		// Ricostruisco la stringa carattere per carattere facendo il mapping inverso dell'alfabeto
		for(unsigned int j=0; j<test_string_tmp.length(); ++j)
		{
			if(test_string_tmp[j] == ' ')
				continue;


			short int mapped_index = geo_alph.find_first_of(test_string_tmp[j]);

			if(mapped_index == -1){
				cerr << "The " << test_string_tmp[j] << " symbol is not inside the Geohash alphabet"<<endl;
				exit(EXIT_FAILURE);
			}

			test_string.push_back(mapped_index);
		}

		//cout << "Stringa iniziale: " << test_string_tmp << endl;
		//cout << "Stringa in test: "<<test_string<<endl;


		if(finaldfa->membership_query(test_string))
		{
			successful += wp[i];

			//cout << "ACC: ";
			//cout << test_string << endl;
		}
		else
		{
			//cout << "NONACC: ";
			//cout << test_string << endl;
		}
	}

	double succ_rate = (double) successful / (double) total_weigth;

	cout << "**** End of generalization test ****"<<endl;

	return succ_rate;
}






// Scrive su file il training set e setta nel vettore "test_set" le stringhe di test
// Ritorna la dimensione del test_set, adoperato per poter fare esternamente i test
// Scrito solo il training perché è sufficiene all'inferenza
// (se vuoi puoi scrivere anche quelle di test nel file, basta copiare e adattare l'ultima riga della funzione)
int geoExp::write_minitraj_from_db_like_samples_TRAINTEST_TESTSET(string user,string prefix, string path_samples)
{
	//int  *wp, *wn;
	//vector<string>::iterator it;

	vector<string> test_set[cross_val_run];
	vector<string> positive_training_set[cross_val_run];


	// Open connection
	geodb* mydb = new geodb(db_path);
	mydb->connect();


	// Positive Samples - minitrajectories for user and prefix
	vector<string>* positive_samples = mydb->get_minitraj_for_prefix_and_user(user, prefix);


	// Negative Samples - minitrajectories for all users, except 'user', and prefix
	vector<string>* negative_samples = mydb->get_minitraj_for_prefix_and_allUsers_EXCEPT_user(user, prefix);


	int dim_positive = positive_samples->size();


	if(dim_positive < MIN_NUMBER_POSITIVE_SAMPLES)
	{
		// Free memory
		for(int i=0; i< cross_val_run; ++i){
			positive_training_set[i].clear();
			test_set[i].clear();
		}

		vector<string>().swap(*positive_samples);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		negative_samples->clear();
		delete positive_samples;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}


	cout << " - Scrivo su file i samples -" << endl;
	cout << "Prefisso: "<<prefix<<endl;
	cout << "Numero sample positivi: "<<positive_samples->size()<<endl;
	cout << "Numero sample negativi: "<<negative_samples->size()<<endl;



	// ****************************************
	// Divido in TRAINING SET e TEST SET 80-20
	// ****************************************

	// Inizzializzo srand
	srand(time(NULL));


	random_shuffle(positive_samples->begin(), positive_samples->end());


	// Calcolo la dimensione del training set e del test set
	int dim_train = ceil( (double) (training_proportion * dim_positive) / (double) 100);
	int dim_test  = dim_positive - dim_train;

	if(dim_positive == 1){
		dim_train = 1;
		dim_test = 0;
	}

	cout << "Tot previste pos: "<<dim_positive<<" | "<<"train: "<<dim_train<<" - test: "<<dim_test<<endl;


	if(dim_test < MIN_NUMBER_TEST_SAMPLES)
	{
		// Free memory
		for(int i=0; i< cross_val_run; ++i){
			vector<string>().swap(positive_training_set[i]);
			vector<string>().swap(test_set[i]);
			positive_training_set[i].clear();
			test_set[i].clear();
		}

		vector<string>().swap(*positive_samples);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		negative_samples->clear();
		delete positive_samples;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}



	////////////////////////////////////////////////////////////////////////
	// Creo il set di TEST e elimino dal training
	if(dim_test != 0)
	{
		for(int i=0; i< cross_val_run; ++i)
		{
			for(auto it=positive_samples->begin(); it != positive_samples->end(); ++it)
			{
				if(test_set[i].size() < dim_test)
					test_set[i].push_back(*it);
				else
					positive_training_set[i].push_back(*it);
			}

			rotate( positive_samples->begin(), positive_samples->begin() + test_set[i].size(),  positive_samples->end());

			// TODO: togliere eventuali duplicati


			cout << "Train finale: "<<positive_training_set[i].size()<<" - testset finale: "<< test_set[i].size() <<endl;
		}
	}




	////////////////////////////////////////////////////////////////////////

	// Scrivo su file
	// (è qui dentro che eventualmente tolgo le RIPETIZIONI interne ad una stringa)
	cout << "Scritture su file dei samples..."<<endl;

	for(int i=0; i<cross_val_run; ++i)
	{
		// Scrivo su file il TRAINIG SET
		string path_training_data = path_samples+ "-"+user+"-samples-CV"+intTostring(i)+".txt";
		write_minitrajectories_as_training_set(&positive_training_set[i] , negative_samples, path_training_data.c_str());


		// Scrivo su file il TEST SET
		string path_test_data = path_samples + "-"+user+"-test_samples-CV"+intTostring(i)+".txt";
		write_minitrajectories_as_test_set(&test_set[i], path_test_data.c_str());
	}


	// Free memory
	for(int i=0; i< cross_val_run; ++i){
		vector<string>().swap(positive_training_set[i]);
		vector<string>().swap(test_set[i]);
		positive_training_set[i].clear();
		test_set[i].clear();
	}

	vector<string>().swap(*positive_samples);
	vector<string>().swap(*negative_samples);
	positive_samples->clear();
	negative_samples->clear();
	delete positive_samples;
	delete negative_samples;

	mydb->close();
	delete mydb;

	return dim_test;
}



// Scrivo tutto il training set nel file. Usato per la similarità tra utenti
void geoExp::write_minitraj_from_db_like_samples_ALL_TRAININGSET(string user,string prefix, string path_samples)
{

	// Open connection
	geodb* mydb = new geodb(db_path);
	mydb->connect();


	// Positive Samples - minitrajectories for user and prefix
	vector<string>* positive_samples = mydb->get_minitraj_for_prefix_and_user(user, prefix);


	// Negative Samples - minitrajectories for all users, except 'user', and prefix
	vector<string>* negative_samples = mydb->get_minitraj_for_prefix_and_allUsers_EXCEPT_user(user, prefix);


	if(positive_samples->size() < MIN_NUMBER_POSITIVE_SAMPLES || negative_samples->size() < MIN_NUMBER_POSITIVE_SAMPLES )
	{
		// Free memory
		vector<string>().swap(*positive_samples);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		negative_samples->clear();
		delete positive_samples;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}


	cout << endl << "Scrivo l'intero trainingset per utente "<< user << ", prefisso: "<<prefix<< endl;
	cout << "Prefisso: "<<prefix<<endl;
	cout << "Numero sample positivi: "<<positive_samples->size()<<endl;
	cout << "Numero sample negativi: "<<negative_samples->size()<<endl;


	////////////////////////////////////////////////////////////////////////
	// Scrivo su file
	// (è qui dentro che eventualmente tolgo le RIPETIZIONI interne ad una stringa)

	string path_training_data = path_samples+ "-"+user+"-samples.txt";
	write_minitrajectories_as_training_set(positive_samples, negative_samples, path_training_data.c_str());



	// Free memory
	vector<string>().swap(*positive_samples);
	vector<string>().swap(*negative_samples);
	positive_samples->clear();
	negative_samples->clear();
	delete positive_samples;
	delete negative_samples;

	mydb->close();
	delete mydb;

}



void geoExp::write_minitraj_from_db_like_samples_PAIR_USERS(string user1, string user2, string prefix, string path_samples)
{

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Open connection
	geodb* mydb = new geodb(db_path);
	mydb->connect();

	// Positive Samples - minitrajectories for user and prefix
	vector<string>* positive_samples = mydb->get_minitraj_for_prefix_and_user(user1, prefix);

	// Positive Samples - minitrajectories for user and prefix
	vector<string>* positive_samples_2 = mydb->get_minitraj_for_prefix_and_user(user2, prefix);

	// Concatenate the two vectors
	positive_samples->insert( positive_samples->end(), positive_samples_2->begin(), positive_samples_2->end() );


	// Negative Samples - minitrajectories for all users, except 'user', and prefix
	vector<string>* negative_samples = mydb->get_minitraj_for_prefix_and_allUsers_EXCEPT_2_users(user1, user2, prefix);



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if(positive_samples->size() < MIN_NUMBER_POSITIVE_SAMPLES || positive_samples_2->size() < MIN_NUMBER_POSITIVE_SAMPLES || negative_samples->size() < MIN_NUMBER_POSITIVE_SAMPLES)
	{
		// Free memory
		vector<string>().swap(*positive_samples);
		vector<string>().swap(*positive_samples_2);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		positive_samples_2->clear();
		negative_samples->clear();

		delete positive_samples;
		delete positive_samples_2;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}


	cout << endl << " - Scrivo su file i samples della COPPIA - " << user1 << ", "<<user2<< endl;
	cout << "Prefisso: "<<prefix<<endl;
	cout << "Numero sample positivi: "<<positive_samples->size()<<endl;
	cout << "Numero sample negativi: "<<negative_samples->size()<<endl;



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Scrivo su file il TRAINIG SET
	// (è qui dentro che eventualmente tolgo le RIPETIZIONI interne ad una stringa)

	string path_training_data = path_samples+ "-PAIR-"+user1+"-"+user2+"-samples.txt";
	write_minitrajectories_as_training_set(positive_samples , negative_samples, path_training_data.c_str());


	// Free memory
	vector<string>().swap(*positive_samples);
	vector<string>().swap(*positive_samples_2);
	vector<string>().swap(*negative_samples);
	positive_samples->clear();
	positive_samples_2->clear();
	negative_samples->clear();

	delete positive_samples;
	delete positive_samples_2;
	delete negative_samples;

	mydb->close();
	delete mydb;
}



// Scrive su file le minitraiettorie giˆ estratte (p_samples) per l'utente ed un prefisso particolare
void geoExp::write_minitrajectories_as_training_set(vector<string>* p_orig_samples, vector<string>* n_orig_samples, const char * file_path)
{

	// Keep a local copy for working
	vector<string> p_samples(*p_orig_samples);
	vector<string> n_samples(*n_orig_samples);


	// Insert space between single characters in a string. This is the iterator
	vector<string>::iterator it;


	// Positive samples
	for (it=p_samples.begin(); it!=p_samples.end(); ++it)
	{
		// Add spaces between characters
		string new_string = add_space_between_characters_delete_repetitions(*it);
		new_string = trimRight(new_string);

		(*it) = new_string;
	}


	// Negative samples
	int count =0;
	bool non_empty_intersection = false;
	for (it=n_samples.begin(); it!= n_samples.end();)
	{

		// Add spaces between characters
		string new_string = add_space_between_characters_delete_repetitions(*it);
		new_string = trimRight(new_string);


		if( find( p_samples.begin(), p_samples.end(), new_string) != p_samples.end() ){
			//cout << "Doppione tra Sample positivo e negativo: "<<new_string<< endl;
			non_empty_intersection = true;
			it = n_samples.erase(it);									// Per ogni run cambia il numero di volte che entra perché il test set è casuale
		} else {

			(*it) = new_string;
			++it;
		}


		// Add new positive samples to map
		//samples[new_string] = 0;
	}


	cout << "Dopo il aver tolto ripetizioni intrastringhe e tolto samples comuni tra pos e neg, i neg sono: "<<n_samples.size() << endl;

	if(non_empty_intersection)
		cout << "Ci sono stati doppioni tra pos e neg"<<endl;



	//////////////////////////////
	// Add to map with weights for samples
	map<string, int> weights_p;
	map<string, int> weights_n;

	for(auto i=p_samples.begin(); i!=p_samples.end(); ++i){
		auto j = weights_p.begin();

		if((j = weights_p.find(*i)) != weights_p.end())				// Controllo che sia definito uno stato per quando entra dopo lambda un elemento dell'alfabeto
			j->second++;
		else
			weights_p[*i] = 1;
	}

	for(auto i=n_samples.begin(); i!=n_samples.end(); ++i){
		auto j = weights_n.begin();

		if((j = weights_n.find(*i)) != weights_n.end())				// Controllo che sia definito uno stato per quando entra dopo lambda un elemento dell'alfabeto
			j->second++;
		else
			weights_n[*i] = 1;
	}




	// **********************
	//   WRITE FILE SAMPLES
	// **********************

	cout << "Scrittura su file del training set (positivi e negativi)"<<endl;


	// Write file with positive and negative samples
	ofstream myfile;
	myfile.open(file_path);


	// Write alphabet size
	myfile << "32" << "\n";

	// Write Geohash alphabet symbols
	myfile << "$ ";									// Empty symbol
	for(int i=0; i<32; ++i)
		myfile << geo_alph[i] << " ";
	myfile << "\n";

	// Write positive samples
	for (auto it=weights_p.begin(); it!=weights_p.end(); ++it)
		if( it->first.length() < MAX_LENGTH_SAMPLES_POS )
			myfile << "+ "+intTostring(it->second)+" "+it->first+"\n";

	// Write negative samples
	for (auto it=weights_n.begin(); it!=weights_n.end(); ++it)
		if( it->first.length() < MAX_LENGTH_SAMPLES_POS )
			myfile << "- "+intTostring(it->second)+" "+it->first+"\n";


	cout << "Su file sono presenti "<<weights_p.size()<<" samples positivi e "<<weights_n.size()<<" negativi"<<endl;

//	// Write positive samples
//	for(It p1=samples.begin(); p1!=samples.end(); ++p1)
//		if((*p1).second  == 1)
//			if( (*p1).first.size() < MAX_LENGTH_SAMPLES_POS)
//				myfile << "+ "+(*p1).first+"\n";
//
//	// Write negative samples
//	for(It p1=samples.begin(); p1!=samples.end(); ++p1)
//		if((*p1).second  == 0)
//			if( (*p1).first.size() < MAX_LENGTH_SAMPLES_NEG)
//				myfile << "- "+(*p1).first+"\n";


	myfile.close();

}


void geoExp::write_minitrajectories_as_test_set(vector<string>* test_samples, const char * file_path)
{

	// Keep a local copy for working
	//vector<string> p_samples(*test_orig_samples);


	// Positive samples
	for (auto it=test_samples->begin(); it!=test_samples->end(); ++it)
	{
		// Add spaces between characters
		string new_string = add_space_between_characters_delete_repetitions(*it);
		new_string = trimRight(new_string);

		(*it) = new_string;
	}


	//////////////////////////////
	// Add to map with weights for samples
	map<string, int> weights_p;

	for(auto i=test_samples->begin(); i!=test_samples->end(); ++i){
		auto j = weights_p.begin();

		if((j = weights_p.find(*i)) != weights_p.end())				// Controllo che sia definito
			j->second++;
		else
			weights_p[*i] = 1;
	}


	// **********************
	//   WRITE FILE SAMPLES
	// **********************

	cout << "Scrittura su file del test set"<<endl;


	// Write file with positive and negative samples
	ofstream myfile;
	myfile.open(file_path);


	// Write alphabet size
	myfile << "32" << "\n";

	// Write Geohash alphabet symbols
	myfile << "$ ";									// Empty symbol
	for(int i=0; i<32; ++i)
		myfile << geo_alph[i] << " ";
	myfile << "\n";

	// Write positive samples
	for (auto it=weights_p.begin(); it!=weights_p.end(); ++it)
		if( it->first.length() < MAX_LENGTH_SAMPLES_POS )
			myfile << "+ "+intTostring(it->second)+" "+it->first+"\n";


	cout << "Su file sono presenti "<<weights_p.size() << " test samples"<<endl;


	myfile.close();

}






// Aggiunge uno spazio tra un carattere e il successivo delle minitraiettorie, per la compatibilità del formato stringhe
string geoExp::add_space_between_characters_delete_repetitions(string old_string)
{
	//cout << "ORIGINALE: " << old_string<<endl;

	string new_string = "";
	int new_length = 0;

	if(!no_repetitions_inside_strings)
	{

		new_length = (  old_string.length() * 2 ) - 1;
		//cout << "Lunghezza nuova stringa: "<<new_length<<endl;

		int index_char = 0;

		char last_char = 'a';

		for(int i=0; i<new_length; ++i)
		{
		   if(i % 2 == 0){

			   new_string = new_string+(old_string)[index_char];
			   last_char = (old_string)[index_char];
			   //cout << "elemento: "<<(old_string)[count]<<endl;
			   index_char++;

		   }else{
			   new_string = new_string+" ";
		   }
		}

	}
	else		// Tolgo qui le RIPETIZIONI interne ad una stringa
	{

		char last_char = 'a';				// Sono sicuro che 'a' non è presente nel geohash

		bool spazio = false;
		for(unsigned int i=0; i<old_string.length(); ++i)
		{
			if(!spazio)
			{
			   if(last_char == (old_string)[i])
				   continue;

			   new_string = new_string+(old_string)[i];
			   last_char = (old_string)[i];

			   spazio = true;
		   }else{
			   new_string = new_string+" ";
			   spazio = false;
		   }
		}
	}

	 //cout << "Nuova stringa: "<<new_string<<endl;

	return new_string;
}


string* geoExp::read_testsamples_leaving_geohash_encoding(const char * /*path*/ path_samples, int &dim_positive, int* &wp)
{
	cout << "Reading strings from txt file: "<<endl;
	int cp = 0;														// Numero di stringhe positive del linguaggio
	char ch;

	char null_symbol;

	map<char, int> mapped_alphabet;
	char* inverse_mapped_alphabet;
	int dim_alphabet;
	//int dim_positive;

	cout << path_samples << endl;

	fstream fin(path_samples, fstream::in);

	if(!fin){
		cerr<<"An error occurred in opening file :("<<endl;
		exit(EXIT_FAILURE);
	}


	// Faccio in conteggio previo del numero di stringhe positive e negative presenti nel txt
	while (fin >> noskipws >> ch) {
		if(ch == '\n')
			continue;
		else if(ch == '+')
			cp++;
	}
	dim_positive = cp;


	string* positive = new string[cp];


	wp	= new int[cp];
	for(int i=0; i<cp; ++i)
		wp[i] = 0;

	cout << intTostring(cp) + " positivi"<<endl;

	int flagcp = 0;
	bool casopositive = false;
	bool primap = true;

	ifstream infile(path_samples);

	bool first = true;
	bool second = false;
	string line;

	while (getline(infile, line))
	{
	    istringstream iss(line);
	    int a;
	    string n;


	    // Read first line for dim alphabet
	    if(first)
	    {
	    	if (!(iss >> a)) { break; } // error
	    	dim_alphabet = a;
	    	//cout << "dimensione alfabeto " << a << endl;
	    	first = false;
	    	second = true;

	    	continue;
	    }


	    // Read second line for alphabet symbol
	    if(second)
	    {
	    	inverse_mapped_alphabet = new char[dim_alphabet];

	    	int counter=-1;
	    	while (iss >> n){
	    		if(counter==-1){
	    			null_symbol = (char) n[0];

	    			++counter;
	    			continue;
	    		}else if(counter>=dim_alphabet)
	    			break;

	    		mapped_alphabet[(char) n[0]] = counter;
	    		inverse_mapped_alphabet[counter++]=n[0];
	    	}

	    	// Alphabet
	    	if(counter!= dim_alphabet){
	    		cerr<<"Error in reading example: number of red alphabet symbols mismatches with the declared one!"<<endl;
	    		cerr<<"Expected symbols: "<<dim_alphabet<<endl;
	    		cerr<<"Reed symbols: "<<counter<<endl;


	    		exit(EXIT_FAILURE);
	    	}

	    	// alphabet ok ;)
	    	second= false;
	    }


	    bool weight = true;
	    bool negative_to_neglect = false;

	    // Read remaining lines. NB:  "iss >> n" read set of characters since next space!
		while (iss >> n)
		{
			if( !n.compare("+") ){

				weight = true;
				casopositive = true;
				if(primap){												// Se è il primo caso evito l'incremento
					primap =false;
					continue;
				}
				flagcp++;
				negative_to_neglect = false;
				continue;

			} else if(  !n.compare("-") ){
				negative_to_neglect = true;
				continue;
			}

			// Se sono in una stringa negativa devo saltare tutto fino alla prossima positiva
			if(negative_to_neglect)
				continue;

			// se la stringa è vuota, non è necessario aggiungere nulla
			if(((char) n[0]) == null_symbol)
				continue;

			// Ho letto il peso, lo aggiungo ai pesi
			if(weight){
				weight = false;

				if(casopositive)
					wp[flagcp] = stringToint(n);

			} else {

				//int tmp = mapped_alphabet[(char) n[0]];

				if(casopositive)
					positive[flagcp] = positive[flagcp] +  n;
			}

		}
	}

	return positive;
}









///////////////////////////////////////////////////////
// SIMILARITY
// Almeno 300 stringhe per tutti i prefissi 4, 17, 25, 41, 62, -68,- 85, 128, 140, 144, 153,
void geoExp::run_inference_similarity()
{
	cout << "Selected algorithms are: " << endl;

	if(edsm)
		cout << "EDSM" << endl;
	if(blueStar)
		cout << "BlueStar"<<endl;


	cout << "Database path: "<<db_path<<endl;


	// Utenti analizzati
	string users[160];
	for(int i=0; i<160; ++i){
		if(i <10)
			users[i] = "00"+intTostring(i);
		else if(i>=10 && i<100)
			users[i] = "0"+intTostring(i);
		else if(i>=100)
			users[i] = intTostring(i);
	}


	// Per Users similarity
	for(int i=0; i<n_compared_users; ++i)
		comp_user[ users[int_comp_user[i]] ] = true;			// Inserisco come chiave la stringa utente e valore se è disponibile


	cout << "Lunghezza minima dei prefissi: " << min_prefixes_length << endl;



	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PREFIXES LENGHT
	for(int j=min_prefixes_length; j<max_prefixes_length; ++j)
	{
		string folder_current_prefix_len = "";
		set<string>::iterator it;
		int limit_prefixes_number = 0;


		cout << endl << endl << endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;
		cout <<  "---------- PREFISSI DI LUNGHEZZA "+intTostring(j)+" -----------"<<endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;

		clock_t tStart = clock();


		// Open connection to DB
		geodb* mydb = new geodb(db_path);
		mydb->connect();


		// Get prefixes used from at least one user
		set<string>* prefixes =mydb->get_prefixes_for_user(comp_user.begin()->first, j);

		for (auto it=next(comp_user.begin(),1); it!=comp_user.end(); ++it)
		{
			set<string>* new_prefixes = mydb->get_prefixes_for_user(it->first, j);
			prefixes->insert( new_prefixes->begin(), new_prefixes->end() );
		}


		mydb->close();
		delete mydb;

		if(prefixes->size() == 0){
			cout << "No prefixes!" << endl;
			continue;
		}


		printf("Time taken to extract prefixesL:%d %.2fs\n",j, (double)(clock() - tStart)/CLOCKS_PER_SEC);


		// Make dir for all prefixes of length 'l'
		folder_current_prefix_len = create_folder(current_exp_folder, intTostring(j), false);


		// Cicla i PREFISSI
		cout << endl << prefixes->size() << " prefixes of length " << j <<", limit to ALL";

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// ALL PREFIXES FOR CURRENT LENGTH
		for (it=prefixes->begin(); it!=prefixes->end() && limit_prefixes_number < PREFIX_NUMBER_LIMIT; ++it)
		{
			cout << endl<< "********  Current prefix: "+*it+" *********" << endl;
			limit_prefixes_number++;


			string*	test_set = NULL;
			geodb*	mydb = NULL;
			string	path_samples = "";
			string	path_training_data = "";
			string 	path_test_data = "";
			string 	path_training_data_pair ="";


			// Current prefix
			string current_prefix = *it;

			// Reset of flags about minimal number of samples for user
			for (auto it_user=comp_user.begin(); it_user!=comp_user.end(); ++it_user)
				it_user->second = true;


			// Cointainer for infered DFA
			map< string, map<string, gi::dfa*>> users_dfa;


			// *********************************
			//     WRITE MINITRAJECTORIES FILE FOR EVERY USER
			// *********************************
			//for(int i=0; i<n_compared_users; ++i)
			for (auto it_user1=comp_user.begin(); it_user1!=comp_user.end(); ++it_user1)
			{
				// START TIME
				std::chrono::time_point<std::chrono::system_clock> start, end;
				start = std::chrono::system_clock::now();

				// Open connection to DB
				mydb = new geodb(db_path);
				mydb->connect();

				// Path per scrivere le minitraiettorie come samples, per il prefisso considerato
				path_samples					= 	folder_current_prefix_len+current_prefix;
				path_training_data			= path_samples + "-"+it_user1->first+"-samples.txt";
				path_training_data_pair	= "";

				try
				{
					// Scrivi le ministraiettorie di training su file di testo e quelle di test nella variabile locale test_set (puntatore)
					write_minitraj_from_db_like_samples_ALL_TRAININGSET(it_user1->first, current_prefix, path_samples);

					if ( !boost::filesystem::exists( path_training_data ) )
						comp_user.at(it_user1->first)  = false;									// Significa che per l'utente corrente non è stato creato il file per samples insufficienti


					for (auto it_user2 = next(it_user1, 1); it_user2!=comp_user.end(); ++it_user2)
					{
						if(comp_user.at(it_user1->first) == false || comp_user.at(it_user2->first) == false )
							continue;

						path_training_data_pair	= path_samples+ it_user1->first+"-"+it_user2->first+"-samples.txt";

						write_minitraj_from_db_like_samples_PAIR_USERS(it_user1->first, it_user2->first, current_prefix, path_samples);
					}

				}
				catch (const char* msg )
				{
					cout << msg << endl << " >-------------------------------< "<< endl;
					mydb->close();
					delete mydb;
					continue;
				}

				mydb->close();
				delete mydb;

				// STOP TIME
				end = std::chrono::system_clock::now();
				std::chrono::duration<double> elapsed_seconds = end-start;
				double elapsed_time = elapsed_seconds.count()*1000.0;
				cout << "Elapsed time in writing minitrajectories: "<<elapsed_time << " ms"<<endl;


			}


			// **********************************
			//   	 INFERENCE - SINGOLO UTENTE
			// **********************************
			cout << endl<< "********  BLUESTAR Prefix:"+current_prefix+" *********" << endl;
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			for (auto it_user=comp_user.begin(); it_user!=comp_user.end(); ++it_user)
			{
				// *********************
				// 	 BLUESTAR algorithm
				// *********************
				gi::dfa* BLUESTAR_dfa;

				string current_user = it_user->first;

				path_training_data	= path_samples + "-"+current_user+"-samples.txt";


				if ( !boost::filesystem::exists( path_training_data ) ) {
					#ifdef DEBUG
					std::cout << "No (succifient) samples for this prefix!" << std::endl;
					#endif
					continue;
				}

				cout << endl << "User: "+it_user->first << endl;

				// Read positive and negative samples
				gi::blueStar* bluestar_exe = new gi::blueStar(path_training_data.c_str(), alpha_value, delta_value);


				// START TIME
				std::chrono::time_point<std::chrono::system_clock> start, end;
				start = std::chrono::system_clock::now();


				// Start inference
				try
				{

					BLUESTAR_dfa = bluestar_exe->run(folder_current_prefix_len+current_prefix+"-");

				}
				catch( const char* msg ){	// NB: Questo potrebbe sfalzare la corrispondenza tra i valori nel file STATISTICS e i prefissi a cui si riferiscono
					cout << msg << "; "<<endl;
					cout << " >---------------------------------------< " << endl;
					continue;
				}

				// Print time taken
				end = std::chrono::system_clock::now();
				std::chrono::duration<double> elapsed_seconds = end-start;
				double elapsed_time = elapsed_seconds.count()*1000.0;
				cout << "Elapsed time for BLUE*: "<<elapsed_time << " ms"<<endl;


				// Record infered DFA
				users_dfa[current_user].insert({current_user, BLUESTAR_dfa});


				// *********************
				// 	    BLUESTAR  print
				// *********************

				// Print transition table of the inferred automaton
				//BLUESTAR_dfa.print_dfa_ttable("- BlueStar dfa -");

				// Create dot figure for the inferred automaton
				string dotBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-"+current_user+"-DOTbluestarALF.dot";
				BLUESTAR_dfa->print_dfa_dot_mapped_alphabet("BlueStar", dotBlueStarpath_alf.c_str());
				cout << "BLUSTAR DFA DOT path: "<<dotBlueStarpath_alf << endl;

				// It prints the inferred automaton in a text file
				string txtBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-"+current_user+"-TXTbluestarALF.txt";
				BLUESTAR_dfa->print_dfa_in_text_file(txtBlueStarpath_alf);
				cout << "BLUSTAR DFA TXT path: "<<dotBlueStarpath_alf << endl;


				// free allocated memory
				if(bluestar_exe!=NULL)
					delete bluestar_exe;
				//if(BLUESTAR_dfa != NULL)
				//	delete BLUESTAR_dfa;

			}// CICLO SUGLI UTENTI





			// **********************************
			//   	 INFERENCE - COPPIE UTENTI
			// **********************************
			cout << endl<< "********  PAIR BLUESTAR Prefix:"+current_prefix+" *********" << endl;
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			for (auto it_user1=comp_user.begin(); it_user1!=comp_user.end(); ++it_user1)
			{
				string current_user1 = it_user1->first;
				for (auto it_user2 = next(it_user1, 1); it_user2!=comp_user.end(); ++it_user2)
				{
					string current_user2 = it_user2->first;

					// *********************
					// 	 BLUESTAR algorithm
					// *********************
					gi::dfa* BLUESTAR_dfa;

					path_training_data_pair = path_samples+ "-PAIR-"+current_user1+"-"+current_user2+"-samples.txt";

					if ( !boost::filesystem::exists( path_training_data_pair ) ) {
						  #ifdef DEBUG
						  std::cout << "No (succifient) samples for this prefix!" << std::endl;
						  #endif
						  continue;
					}

					cout << endl << "Utenti "<<current_user1<<" , "<<current_user2<<endl;


					// Read positive and negative samples
					gi::blueStar* bluestar_exe = new gi::blueStar(path_training_data_pair.c_str(), alpha_value, delta_value);


					// Start inference
					try
					{

						BLUESTAR_dfa = bluestar_exe->run(folder_current_prefix_len+current_prefix+"-");

					}
					catch( const char* msg ){	// NB: Questo potrebbe sfalzare la corrispondenza tra i valori nel file STATISTICS e i prefissi a cui si riferiscono
						cout << msg << "; "<<endl;
						cout << " >---------------------------------------< " << endl;
						continue;
					}

					// Print time taken
					cout<<"Time taken to BLUESTAR: " <<(double)(clock() - tStart)/CLOCKS_PER_SEC << " sec" << endl <<endl;



					// *********************
					// 	    BLUESTAR  print
					// *********************

					// Print transition table of the inferred automaton
					//BLUESTAR_dfa.print_dfa_ttable("- BlueStar dfa -");

					// Create dot figure for the inferred automaton
					string dotBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-DOTbluestarALF-PAIR-"+current_user1+"-"+current_user2+".dot";
					BLUESTAR_dfa->print_dfa_dot_mapped_alphabet("BlueStar", dotBlueStarpath_alf.c_str());
					cout << "BLUSTAR DFA DOT path: "<<dotBlueStarpath_alf << endl;

					// It prints the inferred automaton in a text file
					string txtBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-TXTbluestarALF-PAIR-"+current_user1+"-"+current_user2+"-TXTbluestarALF.txt";
					BLUESTAR_dfa->print_dfa_in_text_file(txtBlueStarpath_alf);
					cout << "BLUSTAR DFA TXT path: "<<dotBlueStarpath_alf << endl;


					// free allocated memory
					if(bluestar_exe!=NULL)
						delete bluestar_exe;
					//if(BLUESTAR_dfa != NULL)
					//	delete BLUESTAR_dfa;


					// Record infered DFA (NCD is symmetrical, save also the symmetrich pair)
					users_dfa[current_user1].insert({current_user2, BLUESTAR_dfa});
					users_dfa[current_user2].insert({current_user1, BLUESTAR_dfa});

				} // CICLO SULLE COPPIE
			}// CICLO SUGLI UTENTI


			// Print similarity matrix
			if(users_dfa.size() != 0){
				write_similarity_matrix(users_dfa,  current_prefix,  path_samples);
				write_similarity_accuracy_on_traininget(users_dfa,  current_prefix,  path_samples);
			}

		}// CICLO I PREFISSI




		//Statistics for a fixed legnth of prefix
		//if(edsm)
			//write_statistics_files(statistics, prefixes->size(), intTostring(user), true, 0);

		//for(int i=0; i<cross_val_run; ++i)
		//	if(blueStar)
		//		write_statistics_files(statistics, prefixes->size(), intTostring(user), false, i);


		//stat_final_results(statistics, prefixes->size(), intTostring(user), false);

		//stat_final_results_minimal(statistics, prefixes->size(), intTostring(user), false);


		prefixes->clear();
		delete prefixes;

	}

}



// Calcola e scrive su file l'accuracy dei DFA accopiati
void geoExp::write_similarity_accuracy_on_traininget(map< string, map<string, gi::dfa*>> users_dfa, string current_prefix, string path_samples)
{

	string file_path = path_samples+"-CheckSimilarita.txt";

	cout << "Scrittura del check di similarità "<< current_prefix << " in: "<< file_path <<endl;

	// Calcolo media e varianza del Accuracy
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > acc;


	// **********************
	//   WRITE FILE SAMPLES
	// **********************

	ofstream myfile;
	myfile.open(file_path);

	myfile << "CHECK DI SIMILARITA on training set' - Prefisso: "<< current_prefix << endl;


	// Scorro tutti gli utenti
	for(auto it_name=comp_user.begin(); it_name != comp_user.end(); ++it_name)
	{
		string cur_user = it_name->first;

		string path_training_data = path_samples+ "-"+cur_user+"-samples.txt";

		if( users_dfa.count(cur_user) != 0 )
		{
			myfile << "----"<< endl << "User "<< cur_user << endl;

			// Scorro la mappa interna per ogni utente
			for(auto it_pair=comp_user.begin(); it_pair != comp_user.end(); ++it_pair)
			{
				string paired_user = it_pair->first;
				if(paired_user == cur_user)
					continue;

				if( users_dfa.at(cur_user).count(paired_user) != 0)
				{
					gi::dfa* paired_dfa = users_dfa.at(paired_user).at(paired_user);

					double succ_rate = make_generalization_test(paired_dfa, path_training_data.c_str());

					acc(succ_rate);

					myfile <<paired_user << "  "<< succ_rate << endl;
				} else {
					myfile << "n"<<endl;
				}
			}
			myfile << endl << endl;

		} else {
			for(int i=0; i< comp_user.size(); ++i)
				myfile << "nan"<<endl;
		}
	}



	myfile << endl << "Media : "<< boost::accumulators::mean(acc) << endl << "Dev Std : "<< sqrt(boost::accumulators::variance(acc)) << endl;
	myfile << "Dev Std norm: "<< sqrt(boost::accumulators::variance(acc))/boost::accumulators::mean(acc) << endl;



	myfile.close();
}



void geoExp::write_similarity_matrix(map< string, map<string, gi::dfa*>> users_dfa, string current_prefix, string path_samples)
{
	double complexity1 = 0, complexity2 = 0, complexity_together = 0;

	string file_path = path_samples+"-MDC.txt";

	cout << "Scrittura della matrice di similarità per prefisso "<< current_prefix << " in: "<< file_path <<endl;


	// Calcolo media e varianza del NCD
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > accNCD;
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > accSTATI;


	// **********************
	//   WRITE FILE SAMPLES
	// **********************

	ofstream myfile;
	myfile.open(file_path);

	myfile << "MATRICE DI SIMILARITA' - Prefisso: "<< current_prefix << endl;


	// Scorro tutti gli utenti
	for(auto it_name=comp_user.begin(); it_name != comp_user.end(); ++it_name)
	{
		string cur_user = it_name->first;

		if( users_dfa.count(cur_user) != 0 )
		{
			myfile << "----"<< endl << "User "<< cur_user << endl;

			// Scorro la mappa interna per ogni utente
			for(auto it_pair=comp_user.begin(); it_pair != comp_user.end(); ++it_pair)
			{
				string paired_user = it_pair->first;
				if(paired_user == cur_user)
					continue;

				if( users_dfa.at(cur_user).count(paired_user) != 0)
				{
					complexity_together =  users_dfa.at(cur_user).at(paired_user)->get_complexity();

					complexity2 = users_dfa.at(paired_user).at(paired_user)->get_complexity();

					complexity1 = users_dfa.at(cur_user).at(cur_user)->get_complexity();

					double curr_NCD = /*fabs(*/ ncd(complexity1, complexity2, complexity_together ) /*)*/;

					accNCD(curr_NCD);

					accSTATI(complexity1);

					accSTATI(complexity2);

					accSTATI(complexity_together);


					myfile <<paired_user << "  "<< curr_NCD << endl;
					//myfile << " ---- " << complexity1 <<", "<< complexity2 <<", "<< complexity_together << endl;
				} else {
					myfile << "n"<<endl;
				}
			}
			myfile << endl << endl;

		} else {
			for(int i=0; i< comp_user.size(); ++i)
				myfile << "nan"<<endl;
		}
	}


	myfile << endl << "Media NCD: "<< boost::accumulators::mean(accNCD) << endl << "Dev Std NCD: "<< sqrt(boost::accumulators::variance(accNCD)) << endl;
	myfile << "Dev Std norm NCD: "<< sqrt(boost::accumulators::variance(accNCD))/boost::accumulators::mean(accNCD) << endl;

	myfile << endl << "Media n stati: "<< boost::accumulators::mean(accSTATI) << endl << "Dev Std n stati: "<< sqrt(boost::accumulators::variance(accSTATI)) << endl;
	myfile << "Dev Std norm n stati: "<< sqrt(boost::accumulators::variance(accSTATI))/boost::accumulators::mean(accSTATI)<< endl;


//	STAMPA DIRETTAMENTE IL NUMERO DI STATI
//	for(auto it_name=users_dfa.begin(); it_name != users_dfa.end(); ++it_name)
//	{
//		myfile << it_name->first;
//		string cur_user = it_name->first;
//		for(auto it = it_name->second.begin(); it!=it_name->second.end(); ++it)
//		{
//			string paired_user = it->first;
//			double num_state =  it->second->get_complexity();
//
//			myfile << "   "<<paired_user << ": "<< num_state <<"  ";
//		}
//		myfile << endl;
//	}


	myfile.close();
}









///////////////////////////////////////////////////////
// COLD START

void geoExp::run_inference_coldstart_similarity()
{
	cout << "Selected algorithms are: " << endl;

	if(blueStar)
		cout << "BlueStar"<<endl;


	cout << "Database path: "<<db_path<<endl;


	// Utenti analizzati
	string users[160];
	for(int i=0; i<160; ++i){
		if(i <10)
			users[i] = "00"+intTostring(i);
		else if(i>=10 && i<100)
			users[i] = "0"+intTostring(i);
		else if(i>=100)
			users[i] = intTostring(i);
	}


	// Per Users similarity
	for(int i=0; i<n_compared_users_coldstart; ++i)
		comp_user_coldstart[i] = users[int_comp_user_coldstart[i]];			// Inserisco come chiave la stringa utente e valore se è disponibile


	cout << "Lunghezza minima dei prefissi: " << min_prefixes_length << endl;


	string current_user = users[user];


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PREFIXES LENGHT
	for(int j=min_prefixes_length; j<max_prefixes_length; ++j)
	{
		string folder_current_prefix_len = "";
		set<string>::iterator it;
		int limit_prefixes_number = 0;

		cout << endl << endl << endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;
		cout <<  "---------- PREFISSI DI LUNGHEZZA "+intTostring(j)+" -----------"<<endl;
		cout << "***************************************************" << endl;
		cout << "***************************************************" << endl;

		clock_t tStart = clock();


		// Open connection to DB
		geodb* mydb = new geodb(db_path);
		mydb->connect();

		// Get prefixes used from at least one user
		set<string>* prefixes =mydb->get_prefixes_for_user(users[user], j);

		mydb->close();
		delete mydb;

		if(prefixes->size() == 0){
			cout << "No prefixes!" << endl;
			continue;
		}


		printf("Time taken to extract prefixesL:%d %.2fs\n",j, (double)(clock() - tStart)/CLOCKS_PER_SEC);


		// Make dir for all prefixes of length 'l'
		folder_current_prefix_len = create_folder(current_exp_folder, intTostring(j), false);


		// Cicla i PREFISSI
		cout << endl << prefixes->size() << " prefixes of length " << j <<", limit to ALL";

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// ALL PREFIXES FOR CURRENT LENGTH
		for (it=prefixes->begin(); it!=prefixes->end() && limit_prefixes_number < PREFIX_NUMBER_LIMIT; ++it)
		{
			cout << endl<< "********  Current prefix: "+*it+" *********" << endl;
			limit_prefixes_number++;


			string*	test_set = NULL;
			geodb*	mydb = NULL;
			string	path_samples = "";
			string	path_training_data = "";
			string 	path_test_data = "";
			string 	path_training_data_pair ="";


			// Current prefix
			string current_prefix = *it;


			// Cointainer for infered DFA
			map< string, gi::dfa*> users_dfa;



			// *********************************
			//     WRITE MINITRAJECTORIES FILE FOR EVERY USER
			// *********************************
			clock_t tStart = clock();


			// Open connection to DB
			mydb = new geodb(db_path);
			mydb->connect();

			// Path per scrivere le minitraiettorie come samples, per il prefisso considerato
			path_samples					= 	folder_current_prefix_len+current_prefix;
			path_training_data			= path_samples + "-"+users[user]+"-samples.txt";
			path_training_data_pair	= "";


			try
			{
				// Scrivi le ministraiettorie di training su file di testo e quelle di test nella variabile locale test_set (puntatore)
				write_minitraj_from_db_like_samples_0_25_TRAINTEST_TESTSET(users[user], current_prefix, path_samples);
			}
			catch (const char* msg ){
				cout << msg << endl << " >-------------------------------< "<< endl;
				mydb->close();
				delete mydb;
				continue;
			}

			mydb->close();
			delete mydb;

			printf("Time taken to write minitrajectories to file: %.2fs\n", (double)(clock() - tStart)/CLOCKS_PER_SEC);
			tStart = clock();



			// ************************************
			//   	 INFERENCE - SINGOLO UTENTE 1/4
			// ************************************
			cout << endl<< "********  BLUESTAR Prefix:"+current_prefix+" *********" << endl;
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			for(int k=0; k<cross_val_run; ++k)
			{
				// *********************
				// 	 BLUESTAR algorithm
				// *********************
				gi::dfa* BLUESTAR_dfa;

				path_training_data = path_samples+ "-"+current_user+"-samples-COLD-CV"+intTostring(k)+".txt";


				if ( !boost::filesystem::exists( path_training_data ) ) {
				//	#ifdef DEBUG
					std::cout << "No (succifient) samples for this prefix!" << std::endl;
			//		#endif
					continue;
				}

				cout << endl << "User: "+current_user  << "Fold: " << intTostring(k) << endl;

				// Read positive and negative samples
				gi::blueStar* bluestar_exe = new gi::blueStar(path_training_data.c_str(), alpha_value, delta_value);


				// Start inference
				try
				{

					BLUESTAR_dfa = bluestar_exe->run(folder_current_prefix_len+current_prefix+"-");

				}
				catch( const char* msg ){	// NB: Questo potrebbe sfalzare la corrispondenza tra i valori nel file STATISTICS e i prefissi a cui si riferiscono
					cout << msg << "; "<<endl;
					cout << " >---------------------------------------< " << endl;
					continue;
				}

				// Print time taken
				cout<<"Time taken to BLUESTAR: " <<(double)(clock() - tStart)/CLOCKS_PER_SEC << " sec" << endl;


				// Record infered DFA
				users_dfa.insert({current_user+"-CV"+intTostring(k), BLUESTAR_dfa});


				// *********************
				// 	    BLUESTAR  print
				// *********************

				// Print transition table of the inferred automaton
				//BLUESTAR_dfa.print_dfa_ttable("- BlueStar dfa -");

				// Create dot figure for the inferred automaton
				string dotBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-"+current_user+"-DOTbluestarALF.dot";
				BLUESTAR_dfa->print_dfa_dot_mapped_alphabet("BlueStar", dotBlueStarpath_alf.c_str());
				cout << "BLUSTAR DFA path: "<<dotBlueStarpath_alf << endl;


				// free allocated memory
				if(bluestar_exe!=NULL)
					delete bluestar_exe;
				//if(BLUESTAR_dfa != NULL)
				//	delete BLUESTAR_dfa;

			}// CICLO SI FOLD




			// **********************************
			//   	 INFERENCE - COPPIE UTENTI
			// **********************************
			cout << endl<< "********  PAIR BLUESTAR Prefix:"+current_prefix+" *********" << endl;
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			for (int i=0; i<n_compared_users_coldstart; ++i)
			{
				string current_user2 = comp_user_coldstart[i];

				for(int k=0; k<cross_val_run; ++k)
				{
					// *********************
					// 	 BLUESTAR algorithm
					// *********************
					gi::dfa* BLUESTAR_dfa;

					path_training_data_pair = path_samples+ "-"+users[user]+"-"+current_user2+"-samples-COLD-CV"+intTostring(k)+".txt";

					if ( !boost::filesystem::exists( path_training_data_pair ) ) {
						//  #ifdef DEBUG
						  std::cout << "No (succifient) samples for this prefix!" << std::endl;
						 // #endif
						  continue;
					}

					cout << endl << "Utenti "<<users[user]<<" , "<<current_user2<<endl;

					// TODO
					// Read positive and negative samples
					gi::blueStar* bluestar_exe = new gi::blueStar(path_training_data_pair.c_str(), alpha_value, delta_value);


					// Start inference
					try
					{

						BLUESTAR_dfa = bluestar_exe->run(folder_current_prefix_len+current_prefix+"-");

					}
					catch( const char* msg ){	// NB: Questo potrebbe sfalzare la corrispondenza tra i valori nel file STATISTICS e i prefissi a cui si riferiscono
						cout << msg << "; "<<endl;
						cout << " >---------------------------------------< " << endl;
						continue;
					}

					// Print time taken
					cout<<"Time taken to BLUESTAR: " <<(double)(clock() - tStart)/CLOCKS_PER_SEC << " sec" << endl <<endl;



					// *********************
					// 	    BLUESTAR  print
					// *********************

					// Print transition table of the inferred automaton
					//BLUESTAR_dfa.print_dfa_ttable("- BlueStar dfa -");

					// Create dot figure for the inferred automaton
					string dotBlueStarpath_alf = folder_current_prefix_len+current_prefix+"-DOTbluestarALF-PAIR-"+users[user]+"-"+current_user2+"-CV"+intTostring(k)+".dot";
					BLUESTAR_dfa->print_dfa_dot_mapped_alphabet("BlueStar", dotBlueStarpath_alf.c_str());
					cout << "BLUSTAR DFA path: "<<dotBlueStarpath_alf << endl;


					// free allocated memory
					if(bluestar_exe!=NULL)
						delete bluestar_exe;
					//if(BLUESTAR_dfa != NULL)
					//	delete BLUESTAR_dfa;


					// Record infered DFA (NCD is symmetrical, save also the symmetrich pair)
					users_dfa.insert({current_user2+"-CV"+intTostring(k), BLUESTAR_dfa});

				} // CICLO SULLE COPPIE
			}// CICLO SUGLI UTENTI


			// Print similarity matrix
			if(users_dfa.size() != 0){
				write_similarity_accuracy_on_testset_COLDSTART(users[user], users_dfa,  current_prefix,  path_samples);
			}

		}// CICLO I PREFISSI




		//Statistics for a fixed legnth of prefix
		//if(edsm)
			//write_statistics_files(statistics, prefixes->size(), intTostring(user), true, 0);

		//for(int i=0; i<cross_val_run; ++i)
		//	if(blueStar)
		//		write_statistics_files(statistics, prefixes->size(), intTostring(user), false, i);


		//stat_final_results(statistics, prefixes->size(), intTostring(user), false);

		//stat_final_results_minimal(statistics, prefixes->size(), intTostring(user), false);


		prefixes->clear();
		delete prefixes;

	}

}



// Scrive il trainingset quarti dell'utente "users" ed il trainingset unione dell'utente quarti e di tutti gli utenti indicati come più simili ad esso
// Questo ripetuto per ogni k-fold

void geoExp::write_minitraj_from_db_like_samples_0_25_TRAINTEST_TESTSET(string user,string prefix, string path_samples)
{

	vector<string> test_set[cross_val_run];
	vector<string> positive_training_set[cross_val_run];


	// Open connection
	geodb* mydb = new geodb(db_path);
	mydb->connect();


	// Positive Samples - minitrajectories for user and prefix
	vector<string>* positive_samples = mydb->get_minitraj_for_prefix_and_user(user, prefix);


	// Negative Samples - minitrajectories for all users, except 'user', and prefix
	vector<string>* negative_samples = mydb->get_minitraj_for_prefix_and_allUsers_EXCEPT_user(user, prefix);


	int dim_positive = positive_samples->size();


	if(dim_positive < MIN_NUMBER_POSITIVE_SAMPLES)
	{
		// Free memory
		for(int i=0; i< cross_val_run; ++i){
			positive_training_set[i].clear();
			test_set[i].clear();
		}

		vector<string>().swap(*positive_samples);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		negative_samples->clear();
		delete positive_samples;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}


	cout << " - Scrivo su file i samples -" << endl;
	cout << "Prefisso: "<<prefix<<endl;
	cout << "Numero sample positivi: "<<positive_samples->size()<<endl;
	cout << "Numero sample negativi: "<<negative_samples->size()<<endl;



	// ****************************************
	// Divido in TRAINING SET e TEST SET 1/4-20
	// ****************************************

	// Calcolo la dimensione del training set e del test set
	int dim_train = ceil( (double) (training_proportion * dim_positive) / (double) 100);
	int dim_test  = dim_positive - dim_train;
	int dim_cold_start_train = ceil( (double) dim_positive * cold_start_proportion);

	if(dim_positive == 1){
		dim_train = 1;
		dim_test = 0;
	}

	if(dim_test < MIN_NUMBER_TEST_SAMPLES)
	{
		// Free memory
		for(int i=0; i< cross_val_run; ++i){
			vector<string>().swap(positive_training_set[i]);
			vector<string>().swap(test_set[i]);
			positive_training_set[i].clear();
			test_set[i].clear();
		}

		vector<string>().swap(*positive_samples);
		vector<string>().swap(*negative_samples);
		positive_samples->clear();
		negative_samples->clear();
		delete positive_samples;
		delete negative_samples;

		mydb->close();
		delete mydb;

		throw "Size of dataset too small ";
	}


	// Inizzializzo srand
	srand(time(NULL));

	random_shuffle(positive_samples->begin(), positive_samples->end());


	cout << "Dimensioni previste: pos: "<<dim_positive<<" | "<<"train: "<<dim_train<<" - test: "<<dim_test<<endl;
	cout << "Dimensione traininset COLD-START: "<<dim_cold_start_train << endl;



	////////////////////////////////////////////////////////////////////////
	// PAIRED USERS
	vector<string>* positive_samples_paired_users[n_compared_users_coldstart][cross_val_run];
	vector<string>* negative_samples_paired_users[n_compared_users_coldstart];


	for(int i=0; i<n_compared_users_coldstart; ++i){
		positive_samples_paired_users[i][0] = mydb->get_minitraj_for_prefix_and_user(comp_user_coldstart[i], prefix);
		negative_samples_paired_users[i] = mydb->get_minitraj_for_prefix_and_allUsers_EXCEPT_2_users(user, comp_user_coldstart[i], prefix);

		for(int k=1; k<cross_val_run; ++k)
			positive_samples_paired_users[i][k] = positive_samples_paired_users[i][0];

	}


	////////////////////////////////////////////////////////////////////////
	// Creo il set di TEST e elimino dal training
	if(dim_test != 0)
	{

		for(int i=0; i< cross_val_run; ++i)
		{
			for(auto it=positive_samples->begin(); it != positive_samples->end(); ++it)
			{
				if(test_set[i].size() < dim_test)
					test_set[i].push_back(*it);
				else if(positive_training_set[i].size() < dim_cold_start_train)					// Riempo quanto necessario per coldstart
					positive_training_set[i].push_back(*it);
			}

			// Rotazione per ottenere positive samples disgiunti tra i cross folders
			rotate( positive_samples->begin(), positive_samples->begin() + test_set[i].size(),  positive_samples->end());

			// TODO: togliere eventuali duplicati

			cout << "CV: "<<i << " Train finale: "<<positive_training_set[i].size()<<" - testset finale: "<< test_set[i].size() <<endl;


			// Paired users training set: union of 1/4users and entire paired user
			for(int k=0; k<n_compared_users_coldstart; ++k)
				positive_samples_paired_users[k][i]->insert( positive_samples_paired_users[k][i]->end(), positive_training_set[i].begin(), positive_training_set[i].end() );

		}

	}



	////////////////////////////////////////////////////////////////////////
	// Scrivo su file
	// (è qui dentro che eventualmente tolgo le RIPETIZIONI interne ad una stringa)
	cout << "Scritture su file dei samples..."<<endl;

	for(int i=0; i<cross_val_run; ++i)
	{
			// Scrivo su file il TRAINING SET
			string path_training_data = path_samples+ "-"+user+"-samples-COLD-CV"+intTostring(i)+".txt";
			write_minitrajectories_as_training_set(&positive_training_set[i] , negative_samples, path_training_data.c_str());


			// Scrivo su file il TEST SET
			string path_test_data = path_samples + "-"+user+"-test_samples-COLD-CV"+intTostring(i)+".txt";
			write_minitrajectories_as_test_set(&test_set[i], path_test_data.c_str());


			// Scrivo su file il TRAINING SET per PAIRED USERS
			for(int k=0; k<n_compared_users_coldstart; ++k)
			{
				string path_training_data = path_samples+ "-"+user+"-"+comp_user_coldstart[k]+"-samples-COLD-CV"+intTostring(i)+".txt";
				write_minitrajectories_as_training_set(positive_samples_paired_users[k][i] , negative_samples_paired_users[k] , path_training_data.c_str());
			}
	}


	// Free memory
	for(int i=0; i< cross_val_run; ++i){
		for(int k=0; k<n_compared_users_coldstart; ++k){
			vector<string>().swap(*(positive_samples_paired_users[k][i]));
			vector<string>().swap(*(negative_samples_paired_users[k]));
			positive_samples_paired_users[k][i]->clear();
			negative_samples_paired_users[k]->clear();
		}

		vector<string>().swap(positive_training_set[i]);
		vector<string>().swap(test_set[i]);
		positive_training_set[i].clear();
		test_set[i].clear();
	}

	vector<string>().swap(*positive_samples);
	vector<string>().swap(*negative_samples);
	positive_samples->clear();
	negative_samples->clear();
	delete positive_samples;
	delete negative_samples;

	mydb->close();
	delete mydb;
}



// Versine COLD-start: Calcola e scrive su file l'accuracy dei DFA accopiati
void geoExp::write_similarity_accuracy_on_testset_COLDSTART(string user, map<string, gi::dfa*> users_dfa, string current_prefix, string path_samples)
{

	string file_path = path_samples+"-CheckSimilarita_COLDSTART.txt";

	cout << "Scrittura del check di similarità "<< current_prefix << " in: "<< file_path <<endl;

	// Calcolo media e varianza del Accuracy
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > cold_stat;
	boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > paired_stat;


	// **********************
	//   WRITE FILE SAMPLES
	// **********************

	ofstream myfile;
	myfile.open(file_path);

	myfile << "CHECK DI SIMILARITA' - Prefisso: "<< current_prefix << endl;


	string current_user1 = user;
	myfile << "----"<< endl << "User "<< current_user1 << endl;


	// Scorro tutti gli utenti
	for(int k=0; k<cross_val_run; ++k)
	{
		myfile << endl << endl <<"---------->Run CV: "<<intTostring(k) << endl;
		if(users_dfa.count(current_user1+"-CV"+intTostring(k)) != 0)
		{
			// ********
			// DFA 1/4
			string path_test_data = path_samples + "-"+current_user1+"-test_samples-COLD-CV"+intTostring(k)+".txt";

			gi::dfa* cold_dfa = users_dfa.at(current_user1+"-CV"+intTostring(k));

			double succ_rate = make_generalization_test(cold_dfa, path_test_data.c_str());

			cold_stat(succ_rate);

			myfile << current_user1 << "  "<< succ_rate << endl;

			cout << "Numero di utenti: "<<n_compared_users << endl;

			// ************
			// DFA UNIONE di 1/4 ed utente simile
			for (int i=0; i<n_compared_users_coldstart; ++i)
			{
				string current_user2 = comp_user_coldstart[i];

				cout << "utente 2 corrente: "<< current_user2 << endl;

				if( users_dfa.count(current_user2+"-CV"+intTostring(k)) != 0 )
				{
						// ********
						// DFA Unione
						gi::dfa* paired_dfa = users_dfa.at(current_user2+"-CV"+intTostring(k));

						double succ_rate_paired = make_generalization_test(paired_dfa, path_test_data.c_str());

						paired_stat(succ_rate_paired);

						myfile <<current_user2 << "  "<< succ_rate_paired << endl;
				} else {
					myfile << "n"<<endl;
				}
			}

			myfile << endl << "Media coldstart: "<< boost::accumulators::mean(cold_stat) << endl << "Dev Std coldstart:  "<< sqrt(boost::accumulators::variance(cold_stat)) << endl;
			myfile << "Dev Std norm coldstart: "<< sqrt(boost::accumulators::variance(cold_stat))/boost::accumulators::mean(cold_stat) << endl;

			myfile << endl << "Media dfa unione: "<< boost::accumulators::mean(paired_stat) << endl << "Dev Std dfa unione: "<< sqrt(boost::accumulators::variance(paired_stat)) << endl;
			myfile << "Dev Std norm dfa unione: "<< sqrt(boost::accumulators::variance(paired_stat))/boost::accumulators::mean(paired_stat) << endl;

		} else {
			for (int i=0; i<n_compared_users_coldstart; ++i)
				myfile << "nan"<<endl;
		}
	}

	myfile.close();
}







///////////////////////////////////////////////////////
// STATISTICS

void geoExp::write_statistics_files(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{

	stat_write_generalization_rate(userstat, n_prefixes, utente, is_edsm, curr_run_cross_val);

	stat_write_num_states(userstat, n_prefixes, utente, is_edsm, curr_run_cross_val);

	stat_write_error_rate_on_trainigset(userstat, n_prefixes, utente, is_edsm, curr_run_cross_val);

	stat_actual_merges(userstat, n_prefixes, utente, is_edsm, curr_run_cross_val);

	stat_heuristic_evaluations(userstat, n_prefixes, utente, is_edsm, curr_run_cross_val);
}



void geoExp::stat_write_generalization_rate(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{
	string file_path = "";

	if(is_edsm)
		file_path = current_exp_folder + "STAT_GENERAL_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_GENERAL_BLUESTAR"+intTostring(curr_run_cross_val)+".txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Generlizzation rate and positive error Blustar:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

			if( userstat[i].percentage_positive_edsm != -1 )
				myfile << userstat[i].percentage_positive_edsm << endl;
			else
				myfile << "-" << endl;

		}
		else if ( userstat[i].percentage_positive_bluestar[curr_run_cross_val] != -1 )				//TODO: !?!?!?! METTI INVECE solo del primo elemento del vettore uno scorrimento
			myfile << userstat[i].percentage_positive_bluestar[curr_run_cross_val]<< endl;
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_write_num_states(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{
	string file_path = "";

	if(is_edsm)
		file_path = current_exp_folder + "STAT_NUM_STATES_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_NUM_STATES_BLUESTAR.txt"+intTostring(curr_run_cross_val)+".txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Number of states:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

			if( userstat[i].num_states_edsm != -1 )
				myfile << userstat[i].num_states_edsm << endl;
			else
				myfile << "-" << endl;

		}
		else if ( userstat[i].num_states_bluestar[curr_run_cross_val] != -1 )
			myfile << userstat[i].num_states_bluestar[curr_run_cross_val]<< endl;
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_write_error_rate_on_trainigset(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{
	string file_path = "";

	if(is_edsm)
		return;
	else
		file_path = current_exp_folder+ "STAT_ERR_RATE_TRAIN_BLUESTAR.txt"+intTostring(curr_run_cross_val)+".txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Error state on training set by BlueStar:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if ( userstat[i].errore_rate_bluestar[curr_run_cross_val] != -1 )
			myfile << userstat[i].errore_rate_bluestar[curr_run_cross_val] << endl;
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_actual_merges(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{
	string file_path = "";

	if(is_edsm)
		file_path = current_exp_folder + "STAT_ACTUAL_MERGES_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_ACTUAL_MERGES_BLUESTAR.txt"+intTostring(curr_run_cross_val)+".txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Number of actual merges:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

			if( userstat[i].num_actual_merges_edsm != -1 )
				myfile << userstat[i].num_actual_merges_edsm << endl;
			else
				myfile << "-" << endl;

		}
		else if ( userstat[i].num_actual_merges_bluestar[curr_run_cross_val] != -1 )
			myfile << userstat[i].num_actual_merges_bluestar[curr_run_cross_val]<< endl;
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_heuristic_evaluations(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val)
{
	string file_path = "";

	if(is_edsm)
		file_path = current_exp_folder + "STAT_HEURISTIC_EVALUATIONS_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_HEURISTIC_EVALUATIONS_BLUESTAR.txt"+intTostring(curr_run_cross_val)+".txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Number of heuristic evaluetions:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

			if( userstat[i].num_heuristic_evaluations_edsm != -1 )
				myfile << userstat[i].num_heuristic_evaluations_edsm << endl;
			else
				myfile << "-" << endl;

		}
		else if ( userstat[i].num_heuristic_evaluations_bluestar[curr_run_cross_val] != -1 )
			myfile << userstat[i].num_heuristic_evaluations_bluestar[curr_run_cross_val]<< endl;
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_final_results(mystat* userstat, int n_prefixes, string utente, bool is_edsm)
{
	string file_path = "";


	if(K_FOLD_CROSS_VAL == 0){
		cerr << "Cross Validation not specificated" << endl;
		return;
	}

	if(is_edsm)
		file_path = current_exp_folder + "STAT_FINAL_RESULTS_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_FINAL_RESULTS_BLUESTAR.txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Media e varianza del Cross Validation:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

		}
		else if ( userstat[i].num_heuristic_evaluations_bluestar[0] != -1 )
		{
			// Accumulatore per i dati
			boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > acc[5];


			// Carico i file nell'accumulators
			for(int k=0; k<K_FOLD_CROSS_VAL; ++k){
				acc[0] ( userstat[i].num_states_bluestar[k] );
				acc[1] ( userstat[i].percentage_positive_bluestar[k] );
				acc[2] ( userstat[i].errore_rate_bluestar[k] );
				acc[3] ( userstat[i].num_actual_merges_bluestar[k] );
				acc[4] ( userstat[i].num_heuristic_evaluations_bluestar[k] );
			}


			// Scrivo su file
			myfile << endl << "MEDIE" << endl;
			myfile << "Num states B: " << boost::accumulators::mean(acc[0]) << endl;
			myfile << "Percentage positive B: " << boost::accumulators::mean(acc[1])  << endl;
			myfile << "Errore Rate B: " << boost::accumulators::mean(acc[2]) << endl;
			myfile << "Num actual merges B: " << boost::accumulators::mean(acc[3]) << endl;
			myfile << "Num heuristic evaluations B: " << boost::accumulators::mean(acc[4])  << endl;


			myfile << endl << "DEV STD" << endl;
			myfile << "Num states B: " << sqrt(boost::accumulators::variance(acc[0]))  << endl;
			myfile << "Percentage positive B: " << sqrt(boost::accumulators::variance(acc[1]))<< endl;
			myfile << "Errore Rate B: " << sqrt(boost::accumulators::variance(acc[2])) << endl;
			myfile << "Num actual merges B: " << sqrt(boost::accumulators::variance(acc[3])) << endl;
			myfile << "Num heuristic evaluations B: " << sqrt(boost::accumulators::variance(acc[4]))<< endl;


			myfile << endl << "DEV STD NORM " << endl;
			myfile << "Num states B: " << sqrt(boost::accumulators::variance(acc[0]))/boost::accumulators::mean(acc[0])   << endl;
			myfile << "Percentage positive B: " << sqrt(boost::accumulators::variance(acc[1]))/boost::accumulators::mean(acc[1]) << endl;
			myfile << "Errore Rate B: " << sqrt(boost::accumulators::variance(acc[2]))/boost::accumulators::mean(acc[2])  << endl;
			myfile << "Num actual merges B: " << sqrt(boost::accumulators::variance(acc[3]))/boost::accumulators::mean(acc[3])  << endl;
			myfile << "Num heuristic evaluations B: " << sqrt(boost::accumulators::variance(acc[4]))/boost::accumulators::mean(acc[4]) << endl;

		}
		else
			myfile << "-" << endl;
	}

	myfile.close();
}



void geoExp::stat_final_results_minimal(mystat* userstat, int n_prefixes, string utente, bool is_edsm)
{
	string file_path = "";


	if(K_FOLD_CROSS_VAL == 0){
		cerr << "Cross Validation not specificated" << endl;
		return;
	}

	if(is_edsm)
		file_path = current_exp_folder + "STAT_MINIMAL_FINAL_RESULTS_EDSM.txt";
	else
		file_path = current_exp_folder+ "STAT_MINIMAL_FINAL_RESULTS_BLUESTAR.txt";

	cout << "Statistics file: "<<file_path << endl;


	// Write in file the positive e negative samples
	ofstream myfile;
	myfile.open(file_path.c_str(), ios::app);

	myfile << endl << endl << "Utente: "<< utente << " - Lunghezza prefisso: "<< intTostring(userstat[0].prefix.length()) << endl;
	myfile << "Media e varianza del Cross Validation:" << endl;

	for(int i=0; i<n_prefixes; ++i)
	{
		if(is_edsm){

		}
		else if ( userstat[i].num_heuristic_evaluations_bluestar[0] != -1 )
		{
			// Accumulatore per i dati
			boost::accumulators::accumulator_set<double, boost::accumulators::features<boost::accumulators::tag::mean, boost::accumulators::tag::variance> > acc[5];


			// Carico i file nell'accumulators
			for(int k=0; k<K_FOLD_CROSS_VAL; ++k){
				acc[0] ( userstat[i].num_states_bluestar[k] );
				acc[1] ( userstat[i].percentage_positive_bluestar[k] );
				acc[2] ( userstat[i].errore_rate_bluestar[k] );
				acc[3] ( userstat[i].num_actual_merges_bluestar[k] );
				acc[4] ( userstat[i].num_heuristic_evaluations_bluestar[k] );
			}


			// Scrivo su file
			myfile << endl << "MEDIE" << endl;
			myfile << boost::accumulators::mean(acc[0]) << endl;
			myfile << boost::accumulators::mean(acc[1])  << endl;
			myfile << boost::accumulators::mean(acc[2]) << endl;
			myfile << boost::accumulators::mean(acc[3]) << endl;
			myfile << boost::accumulators::mean(acc[4])  << endl;


			myfile << endl << "DEV STD" << endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[0]))  << endl;
			myfile << sqrt(boost::accumulators::variance(acc[1]))<< endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[2])) << endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[3])) << endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[4]))<< endl;


			myfile << endl << "DEV STD NORM " << endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[0]))/boost::accumulators::mean(acc[0])   << endl;
			myfile <<  sqrt(boost::accumulators::variance(acc[1]))/boost::accumulators::mean(acc[1]) << endl;
			myfile << sqrt(boost::accumulators::variance(acc[2]))/boost::accumulators::mean(acc[2])  << endl;
			myfile << sqrt(boost::accumulators::variance(acc[3]))/boost::accumulators::mean(acc[3])  << endl;
			myfile << sqrt(boost::accumulators::variance(acc[4]))/boost::accumulators::mean(acc[4]) << endl;

		}
		else
			myfile << "-" << endl;
	}

	myfile.close();
}




void geoExp::print_execution_parent_folder()
{
	cout<<"Execution dir: "<< exec_path <<endl;
}

void geoExp::print_experiments_folder()
{
	cout<<"Experiments dir: "<< root_exp_path <<endl;
}


void geoExp::get_num_trajectories_for_pref_length(int prefixes_length)
{
	// Utenti analizzati
	string users1[160];
	for(int i=0; i<160; ++i){
		if(i <10)
			users1[i] = "00"+intTostring(i);
		else if(i>=10 && i<100)
			users1[i] = "0"+intTostring(i);
		else if(i>=100)
			users1[i] = intTostring(i);
	}



	// Open connection to DB
	geodb* mydb = new geodb(db_path);
	mydb->connect();


	// IL numero viene automaticamente stampato dalla funzione del DB!
	for(int i=0; i<160; ++i)
		mydb->get_num_minitraj_for_user(users1[i], prefixes_length);



	mydb->close();
	delete mydb;

}
