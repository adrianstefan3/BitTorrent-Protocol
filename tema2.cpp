#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

using namespace std;

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

// Tag-uri pentru comunicarea intre clienti/tracker
#define download 1
#define d_client 8
#define download_done 3
#define update 4
#define stop_upload 5
#define done_initialize 6
#define d_upload 7

// Informatia pe care o detine tracker-ul
struct info_tracker {
    map<string, vector<string>> files;
    map<string, vector<int>> swarm;
} info_t;

// Informatia pe care o detine un client
struct info_client {
    int n_f;
    map<string, vector<string>> files;
    vector<string> w_files;
} info_c;

// Functie care citeste informatile pe care
// le detine un client din fisierul corespunzator
// lui
void read_file(string path) {
    ifstream fin(path);
    if (!fin.is_open()) {
        return;
    }

    fin >> info_c.n_f;

    for (int i = 0; i < info_c.n_f; i++) {
        string file_name;
        int seg_num;

        fin >> file_name >> seg_num;

        vector<string> segm;
        for (int j = 0; j < seg_num; j++) {
            string segment;
            fin >> segment;
            segm.push_back(segment);
        }

        info_c.files[file_name] = segm;
    }

    int n_w_files;
    fin >> n_w_files;

    for (int i = 0; i < n_w_files; i++) {
        string file_name;
        fin >> file_name;
        info_c.w_files.push_back(file_name);
    }

    fin.close();
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    string path = "in" + to_string(rank) + ".txt";

    // Citire fisier
    read_file(path);

    // Trimitere date catre tracker
    MPI_Send(&(info_c.n_f), 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    for (const auto& entry : info_c.files) {
        MPI_Send(entry.first.data(), entry.first.size(), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        int n_segm = entry.second.size();
        MPI_Send(&n_segm, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        for (const auto& segm : entry.second) {
            MPI_Send(segm.data(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        }
    }

    int ACK = 0;
    MPI_Recv(&ACK, 1, MPI_INT, TRACKER_RANK, done_initialize, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Incepere descarcare fisiere
    for (const auto& wanted_file : info_c.w_files) {
        int val = 1;
        // Anunt tracker-ul ca vreau sa descarc un fisier
        MPI_Send(&val, 1, MPI_INT, TRACKER_RANK, download, MPI_COMM_WORLD);
        MPI_Send(wanted_file.data(), wanted_file.size(), MPI_CHAR, TRACKER_RANK, download, MPI_COMM_WORLD);
        int n_segm, n_clients;
        MPI_Recv(&n_segm, 1, MPI_INT, TRACKER_RANK, download, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&n_clients, 1, MPI_INT, TRACKER_RANK, download, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        vector<int> swarm(n_clients);
        MPI_Recv(swarm.data(), n_clients, MPI_INT, TRACKER_RANK, download, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Primirea hash-urilor pentru fiecare segment
        vector<string> segments;
        for (int i = 0; i < n_segm; i++) {
            string segm(HASH_SIZE, '\0');
            MPI_Recv(&segm[0], HASH_SIZE, MPI_CHAR, TRACKER_RANK, download, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            segments.push_back(segm);
        }

        int contor_segmente = 0;
        for (int i = 0; i < n_segm; i++) {
            int client = rand() % n_clients;
            while (swarm[client] == rank) {
                client = rand() % n_clients;
            }
            int ACK = 0;
            string segm = segments[i];
            int size_name = wanted_file.size();
            MPI_Send(&size_name, 1, MPI_INT, swarm[client], d_upload, MPI_COMM_WORLD);
            // Folosesc d_client + rank pentru ca un client de rank "x"
            // poate face atat download cat si upload si nu trebuie sa se
            // intersecteze "canalele" de comunicatie cu ceilalti clienti
            MPI_Send(wanted_file.data(), size_name, MPI_CHAR, swarm[client], d_client + rank, MPI_COMM_WORLD);
            MPI_Send(segm.data(), HASH_SIZE, MPI_CHAR, swarm[client], d_client + rank, MPI_COMM_WORLD);
            MPI_Recv(&ACK, 1, MPI_INT, swarm[client], d_client + rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (ACK == 1) {
                info_c.files[wanted_file].push_back(segm);
                contor_segmente++;
                if (contor_segmente == 10) {
                    // Actualizarea swarm-ului
                    swarm.clear();
                    int ok = 1;
                    MPI_Send(&ok, 1, MPI_INT, TRACKER_RANK, update, MPI_COMM_WORLD);
                    MPI_Send(wanted_file.data(), wanted_file.size(), MPI_CHAR, TRACKER_RANK, update, MPI_COMM_WORLD);
                    MPI_Recv(&n_clients, 1, MPI_INT, TRACKER_RANK, update, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    for (int j = 0; j < n_clients; j++) {
                        int aux;
                        MPI_Recv(&aux, 1, MPI_INT, TRACKER_RANK, update, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        swarm.push_back(aux);
                    }
                    contor_segmente = 0;
                }
            } else {
                i--;
            }
        }

        // Afisare in fisier
        string path = "client" + to_string(rank) + "_" + wanted_file;
        ofstream fout(path);
        for (const auto& hash : info_c.files[wanted_file]) {
            fout << hash << endl;
        }
        fout.close();
    }

    // Finalizare descarcare fisiere
    ACK = 1;
    MPI_Send(&ACK, 1, MPI_INT, TRACKER_RANK, download_done, MPI_COMM_WORLD);
    
    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    while (true) {
        int val;
        MPI_Status status;
        MPI_Recv(&val, 1, MPI_INT, MPI_ANY_SOURCE, d_upload, MPI_COMM_WORLD, &status);
        if (val != -1) {
            // Folosesc d_client + rank pentru ca un client de rank "x"
            // poate face atat download cat si upload si nu trebuie sa se
            // intersecteze "canalele" de comunicatie cu ceilalti clienti
            string file_name(val, '\0');
            MPI_Recv(&file_name[0], val, MPI_CHAR, status.MPI_SOURCE, d_client + status.MPI_SOURCE, MPI_COMM_WORLD, &status);
            string hash(HASH_SIZE, '\0');
            MPI_Recv(&hash[0], HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, d_client + status.MPI_SOURCE, MPI_COMM_WORLD, &status);
            
            // Cautare hash segment
            int ok = 0;
            if (info_c.files.find(file_name) != info_c.files.end()) {
                for (const auto& h : info_c.files[file_name]) {
                    if (h == hash) {
                        ok = 1;
                        break;
                    }
                }
            }

            MPI_Send(&ok, 1, MPI_INT, status.MPI_SOURCE, d_client + status.MPI_SOURCE, MPI_COMM_WORLD);
        } else {
            break;
        }
    }

    return NULL;
}

void tracker(int numtasks, int rank) {
    for (int k = 1; k < numtasks; k++) {
        int n_files;
        MPI_Recv(&n_files, 1, MPI_INT, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int i = 0; i < n_files; i++) {
            string file_name(MAX_FILENAME, '\0');
            vector<string> segmente;

            // Recv numele fisierului
            MPI_Recv(&file_name[0], MAX_FILENAME, MPI_CHAR, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int n_segm;
            MPI_Recv(&n_segm, 1, MPI_INT, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Recv segmentele corespunzatoare fisierului curent
            for (int j = 0; j < n_segm; j++) {
                string segment(HASH_SIZE, '\0');
                MPI_Recv(&segment[0], HASH_SIZE, MPI_CHAR, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                segmente.push_back(segment);
            }

            // Retinere nume fisier + has-uri in tracker doar
            // a fisierelor care nu au fost retinute deja
            if (info_t.files.find(file_name) == info_t.files.end()) {
                info_t.files[file_name] = segmente;
            }

            info_t.swarm[file_name].push_back(k);
        }
    }

    // Anunt ca am primit informatiile de la clienti
    int ACK = 1;
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ACK, 1, MPI_INT, i, done_initialize, MPI_COMM_WORLD);
    }
    
    int finished_clients = 0;
    while (true) {
        MPI_Status status;
        int val;
        MPI_Recv(&val, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == download) {
            string file_name(MAX_FILENAME, '\0');
            MPI_Recv(&file_name[0], MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, download, MPI_COMM_WORLD, &status);
            int n_segm = info_t.files[file_name].size();
            MPI_Send(&n_segm, 1, MPI_INT, status.MPI_SOURCE, download, MPI_COMM_WORLD);
            int n_clients = info_t.swarm[file_name].size();
            MPI_Send(&n_clients, 1, MPI_INT, status.MPI_SOURCE, download, MPI_COMM_WORLD);
            MPI_Send(info_t.swarm[file_name].data(), n_clients, MPI_INT, status.MPI_SOURCE, download, MPI_COMM_WORLD);

            for (const auto& hash : info_t.files[file_name]) {
                MPI_Send(hash.data(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, download, MPI_COMM_WORLD);
            }

            info_t.swarm[file_name].push_back(status.MPI_SOURCE);
        } else if (status.MPI_TAG == download_done) {
            finished_clients++;
            if (finished_clients == numtasks - 1) {
                break;
            }
        } else if (status.MPI_TAG == update) {
            string file_name(MAX_FILENAME, '\0');
            MPI_Recv(&file_name[0], MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, update, MPI_COMM_WORLD, &status);
            int n_clients = info_t.swarm[file_name].size();
            MPI_Send(&n_clients, 1, MPI_INT, status.MPI_SOURCE, update, MPI_COMM_WORLD);
            int client;
            for (int i = 0; i < n_clients; i++) {
                client = info_t.swarm[file_name][i];
                MPI_Send(&client, 1, MPI_INT, status.MPI_SOURCE, update, MPI_COMM_WORLD);
            }
        }
    }

    ACK = -1;
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ACK, 1, MPI_INT, i, d_upload, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}

