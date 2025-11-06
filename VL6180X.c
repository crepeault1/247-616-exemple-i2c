#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h> //for IOCTL defs
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>

// #define I2C_BUS "/dev/i2c-0" // fichier Linux representant le BUS #0
#define I2C_BUS "/dev/i2c-1" // fichier Linux representant le BUS #1

/*******************************************************************************************/
/*******************************************************************************************/
#define CAPTEUR_I2C_ADDRESS 0x29 // adresse I2C du capteur de distance
#define CAPTEUR_REGID 0x000      // adresse du registre ID du capteur de distance
#define REGISTRE_DEPART 0x018    //Registre pour le lancement d'une conversion
#define REGISTRE_LECTURE 0x062   //Registre des données de conversion

#define SINGLESHOT 0x01
#define CONTINUOUS 0x02

/*******************************************************************************************/
/*******************************************************************************************/

int setupPortI2C(void);
int Lire_ID_Capteur(int);
void VL6180X_Tuning(int);
uint8_t lireCapteur(void);
int fdPortI2C;

uint8_t Registre16bitLecture[2];    
uint8_t Registre16bitDepart[3];

int main()
{
    uint8_t lectureCapteur;

    //Pipes
    int pipe_P_E[2];  //Parent à Enfant
    int pipe_E_PE[2]; //Enfant à Petit-Enfant
    int pipe_PE_E[2]; //Petit-Enfant à Enfant
    int pipe_E_P[2];  //Enfant à Parent
    //
    //  _________  [1]                [0]  _________  [1]                 [0]  _________
    // |         | ----- pipe_P_E ------> |         | ----- pipe_E_PE ------> |         |
    // |  Parent |                        | Enfant  |                         | Petit-  |
    // |         | <---- pipe_E_P ------- |         | <---- pipe_PE_E ------- | enfant  |
    // |_________| [0]                [1] |_________| [0]                 [1] |_________|
    //

    //Flags pour rendre les pipes non-bloquants
    int flags1 = fcntl(pipe_P_E[0], F_GETFL, 0);
    int flags2 = fcntl(pipe_E_PE[0], F_GETFL, 0);
    int flags3 = fcntl(pipe_PE_E[0], F_GETFL, 0);
    int flags4 = fcntl(pipe_E_P[0], F_GETFL, 0);

    //Setup capteur
    setupPortI2C();
    printf("ID CAPTEUR = %#04x\n", Lire_ID_Capteur(fdPortI2C));
    VL6180X_Tuning(fdPortI2C);

    //Préparation des pipes pour le fork
    if ((pipe(pipe_P_E) == -1) || 
        (pipe(pipe_E_PE) == -1) ||
        (pipe(pipe_PE_E) == -1) || 
        (pipe(pipe_E_P) == -1)
       )
    {
        perror("pipe");
    }

    //Changement de flags pour rendre les pipes non-blocants
    fcntl(pipe_P_E[0], F_SETFL, flags1 | O_NONBLOCK);
    fcntl(pipe_E_PE[0], F_SETFL, flags2 | O_NONBLOCK);
    fcntl(pipe_PE_E[0], F_SETFL, flags3 | O_NONBLOCK);
    fcntl(pipe_E_P[0], F_SETFL, flags4 | O_NONBLOCK);

    //Debut du fork
    pid_t pid = fork();
    if(pid < 0)
    {
        printf("Fork failed.\n");
    }
    else if (pid == 0)
    {
        pid_t pid2 = fork();
        if (pid2 < 0)
        {
            printf("Grandchild fork failed.\n");
        }
        else if (pid2 == 0)
        {
            //************************* PETIT-ENFANT **************************
            char messageEnfant;
            uint8_t distanceCapteur;
            int Mode = 0;
            //Fermeture des copies de pipes inutiles reçues dans le fork()
            close(pipe_P_E[0]);
            close(pipe_P_E[1]);
            close(pipe_E_P[0]);
            close(pipe_E_P[1]);
                 
            while(1)
            {
                if (read(pipe_E_PE[0], &messageEnfant, 1) > 0)
                {
                    switch (messageEnfant)
                    {
                    case 'm':
                        Mode = 1;
                        //printf("TestDebug\n");
                        break;
                    case 's':
                        Mode = 0;
                        
                        break;
                    case 'q':
                        printf("Fin du processus Petit-enfant.\n");
                        sleep(1);
                        fflush(stdout);
                        write(pipe_E_PE[1], "q", 1);
                        _exit(0);
                        break;
                    default:
                        continue;
                    }
                    if (Mode == 1)
                    {
                        distanceCapteur = lireCapteur();
                        write(pipe_PE_E[1], &distanceCapteur, 1);
                    }
                }
            }
        }
        else
        {
            //**************************** ENFANT *****************************
            uint8_t messageDuParent;
            uint8_t messageDuPetitEnfant;
            while(1)
            {
                // Lecture des messages du parent et envoi au petit-enfant
                read(pipe_P_E[0], &messageDuParent, 1);
                write(pipe_E_PE[1], &messageDuParent, 1);
                if (messageDuParent == 'm')
                {
                    // Lecture des messages du petit enfant
                    if(read(pipe_PE_E[0], &messageDuPetitEnfant, 1) > 0)
                    {
                        if (messageDuPetitEnfant == 'q')
                        {
                            printf("Fin du processus Enfant.\n");
                            write(pipe_E_P[1], "q", 1);
                            wait(NULL);
                            _exit(0);
                        }
                        else
                        {
                            printf("Lecture du capteur: %03d\n", messageDuPetitEnfant);
                            usleep(100000);
                        }
                    }
                    else
                    {
                        usleep(1000);
                    }
                }
            }
        }
    }
    else
    {
        //****************************** PARENT *******************************
        char inputTerminal;
        //Fermeture des pipes inutiles reçues dans le fork() (enfant-petit enfant)
        close(pipe_E_PE[0]);
        close(pipe_E_PE[1]);
        close(pipe_PE_E[0]);
        close(pipe_PE_E[1]);

        while(inputTerminal != 'q')
        {
            inputTerminal = getchar();
            if((inputTerminal == 'm') || (inputTerminal == 's'))
            {
                write(pipe_P_E[1], &inputTerminal, 1);
            }
        }
        wait(NULL);
        printf("Fin du processus Parent. Programme terminé.\n");
    }
    close(fdPortI2C);
    return 0;
}

int setupPortI2C(void)
{
    Registre16bitLecture[0] = (uint8_t)(REGISTRE_LECTURE >> 8);
    Registre16bitLecture[1] = (uint8_t)REGISTRE_LECTURE;
    
    Registre16bitDepart[0] = (uint8_t)(REGISTRE_DEPART >> 8);
    Registre16bitDepart[1] = (uint8_t)REGISTRE_DEPART;
    Registre16bitDepart[2] = (uint8_t)SINGLESHOT;

    fdPortI2C = open(I2C_BUS, O_RDWR); // ouverture du 'fichier', création d'un 'file descriptor' vers le port I2C
    if (fdPortI2C == -1)
    {
        perror("erreur: ouverture du port I2C ");
        return 1;
    }

    /// Liaison de l'adresse I2C au fichier (file descriptor) du bus I2C et Initialisation
    if (ioctl(fdPortI2C, I2C_SLAVE_FORCE, CAPTEUR_I2C_ADDRESS) < 0)
    { // I2C_SLAVE_FORCE if it is already in use by a driver (i2cdetect : UU)
        perror("erreur: adresse du device I2C ");
        close(fdPortI2C);
        return 1;
    }
}

int Lire_ID_Capteur(int fdPortI2C)
{
    uint8_t Identification; // emplacement memoire pour stocker la donnee lue
    uint8_t Registre16bit[2];
    usleep(2000);
    Registre16bit[0] = (uint8_t)(CAPTEUR_REGID >> 8);
    Registre16bit[1] = (uint8_t)CAPTEUR_REGID;

    if (write(fdPortI2C, Registre16bit, 2) != 2)
    {
        perror("erreur: I2C_ecrire ");
        return -1;
    }
    if (read(fdPortI2C, &Identification, 1) != 1)
    {
        perror("erreur: I2C_Lire ");
        return -1;
    }
    return Identification;
}

void VL6180X_Tuning(int fdI2C)
{
    uint8_t returnVal;
    uint8_t AddressDataI2C[3];
    uint16_t tuningValues[40][2] = {{0x0207, 0x01},
                                    {0x0208, 0x01},
                                    {0x0096, 0x00},
                                    {0x0097, 0xFD},
                                    {0x00E3, 0x00},
                                    {0x00E4, 0x04},
                                    {0x00E5, 0x02},
                                    {0x00E6, 0x01},
                                    {0x00E7, 0x03},
                                    {0x00F5, 0x02},
                                    {0x00D9, 0x05},
                                    {0x00DB, 0xCE},
                                    {0x00DC, 0x03},
                                    {0x00DD, 0xF8},
                                    {0x009F, 0x00},
                                    {0x00A3, 0x3C},
                                    {0x00B7, 0x00},
                                    {0x00BB, 0x3C},
                                    {0x00B2, 0x09},
                                    {0x00CA, 0x09},
                                    {0x0198, 0x01},
                                    {0x01B0, 0x17},
                                    {0x01AD, 0x00},
                                    {0x00FF, 0x05},
                                    {0x0100, 0x05},
                                    {0x0199, 0x05},
                                    {0x01A6, 0x1B},
                                    {0x01AC, 0x3E},
                                    {0x01A7, 0x1F},
                                    {0x0030, 0x00},
                                    {0x0011, 0x10},
                                    {0x010A, 0x30},
                                    {0x003F, 0x46},
                                    {0x0031, 0xFF},
                                    {0x0040, 0x63},
                                    {0x002E, 0x01},
                                    {0x001B, 0x09},
                                    {0x003E, 0x31},
                                    {0x0014, 0x24},
                                    {0x0016, 0x00}};

    //Clear "Fresh Out of Reset" flag
    uint8_t addr[2] = {0x00, 0x16};
    uint8_t clear = 0x00;
    write(fdI2C, addr, 2);
    write(fdI2C, &clear, 1);

    for (uint8_t i = 0; i < 40; i++)
    {
        //Casse l'adresse 16 bits en deux
        AddressDataI2C[0] = (uint8_t)(tuningValues[i][0] >> 8); //Address High
        AddressDataI2C[1] = (uint8_t)(tuningValues[i][0]);      //Address Low
        AddressDataI2C[2] = (uint8_t)(tuningValues[i][1]);      //Data

        //Ecriture du registre d'ecriture (2 bytes)
        returnVal = write(fdI2C, AddressDataI2C, 3);
        if(returnVal != 3)
        {
            perror("Tuning write operation");
        }
    }
    printf("Tuning success. Press 'm' to measure and 's' to stop.\n");
    sleep(1);
}

uint8_t lireCapteur(void)
{
    uint8_t distanceCapteur;
    // Lance une conversion
    write(fdPortI2C, Registre16bitDepart, 3);

    // Sleep pendant la conversion
    usleep(500);

    // Lecture du registre de data
    write(fdPortI2C, Registre16bitLecture, 2);
    read(fdPortI2C, &distanceCapteur, 1);

    return distanceCapteur;
}