#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h> //for IOCTL defs
#include <fcntl.h>
#include <sys/wait.h>

// #define I2C_BUS "/dev/i2c-0" // fichier Linux representant le BUS #0
#define I2C_BUS "/dev/i2c-1" // fichier Linux representant le BUS #1

/*******************************************************************************************/
/*******************************************************************************************/
#define CAPTEUR_I2C_ADDRESS 0x29 // adresse I2C du capteur de distance
#define CAPTEUR_REGID 0x000      // adresse du registre ID du capteur de distance
#define REGISTRE_DEPART 0x018

#define SINGLESHOT 0x01
#define CONTINUOUS 0x02

#define REGISTRE_LECTURE 0x062
/*******************************************************************************************/
/*******************************************************************************************/

int Lire_ID_Capteur(int);
void VL6180X_Tuning(int);

int main()
{
    int fdPortI2C; // file descriptor I2C
    uint8_t Registre16bitLecture[2];
    Registre16bitLecture[0] = (uint8_t)(REGISTRE_LECTURE >> 8);
    Registre16bitLecture[1] = (uint8_t)REGISTRE_LECTURE;

    uint8_t Registre16bitDepart[3];
    Registre16bitDepart[0] = (uint8_t)(REGISTRE_DEPART >> 8);
    Registre16bitDepart[1] = (uint8_t)REGISTRE_DEPART;
    Registre16bitDepart[2] = (uint8_t)0x01;

    uint8_t RegistreLectureEcriture[2] = {SINGLESHOT, CONTINUOUS};
    uint8_t lectureCapteur;
    uint8_t SingleShot = 0x01;

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

    //Lecture du ID du capteur (normalement 0xB4)
    printf("ID CAPTEUR = %#04x\n", Lire_ID_Capteur(fdPortI2C));

    //Envoi des informations de preparation a l'utilisation du VL6180X
    VL6180X_Tuning(fdPortI2C);

    while(1)
    {
        // trigger measurement
        write(fdPortI2C, Registre16bitDepart, 3);

        usleep(500);

        // read range result
        write(fdPortI2C, Registre16bitLecture, 2);
        read(fdPortI2C, &lectureCapteur, 1);

        // clear interrupt
        uint8_t clrAddr[2] = {0x00, 0x15};
        uint8_t clr = 0x07;
        write(fdPortI2C, clrAddr, 2);
        write(fdPortI2C, &clr, 1);

        //Ecriture au registre de depart (0x018)
        //write(fdPortI2C, Registre16bitDepart, 2);
        //write(fdPortI2C, &SingleShot, 1);

        //Lecture au registre de lecture (0x062)
        //write(fdPortI2C, Registre16bitLecture, 2);
        //read(fdPortI2C, &lectureCapteur, 1);
        printf("Lecture du capteur: %02d\n", lectureCapteur);
        sleep(1);
    }
    close(fdPortI2C);
    return 0;
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
    printf("Tuning success!\n");
    sleep(1);
}