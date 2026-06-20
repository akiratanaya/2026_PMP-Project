#include "dict.h"
#include <avr/pgmspace.h>
#include <string.h>

typedef struct
{
    char code[DICT_CODE_SIZE];
    char full[DICT_FULL_SIZE];
} dict_entry_t;

static const dict_entry_t PROGMEM name_dict[] =
{
    //Electronics & IT
    { "MBP", "MacBook Pro"      },
    { "MON", "Monitor"          },
    { "KBD", "Keyboard"         },
    { "MSE", "Mouse"            },
    { "PRJ", "Proyektor"        },
    { "PRN", "Printer"          },
    { "RTR", "Router"           },
    { "SWT", "Switch"  },
    { "SVR", "Server"           },
    { "UPS", "UPS"              },
    { "LAN", "Kabel LAN"        },
    { "HDM", "Kabel HDMI"       },
    { "CAM", "Kamera"           },
    { "TLP", "Telepon"          },
    { "FGR", "Fingerprint"      },
    { "PSD", "Password Device"  },
    { "BTR", "Baterai"          },
    { "CAB", "Kabel"            },
    { "USB", "Kabel USB"        },
    { "SDC", "SD Card"          },
    { "FLS", "Flashdisk"        },
    { "HDD", "Hard Disk Drive"  },
    { "SSD", "Solid State Drive"},
    { "RAM", "Memori RAM"       },
    { "CPU", "Prosesor (CPU)"   },
    { "GPU", "Kartu GPU" },
    { "FAN", "Kipas"  },
    { "PSU", "Power Supply"     },
    { "CSG", "Casing PC"        },
    { "MBD", "Motherboard"      },
    { "SPK", "Speaker"          },
    { "MIC", "Microphone"       },
    { "HS",  "Headset"          },
    { "WBC", "Webcam"           },
    { "SCA", "Scanner"          },
    { "FAX", "Mesin Fax"        },
    { "CPY", "Fotokopi"   },
    { "SHD", "Mesin Penghancur" },
    { "KLC", "Kalkulator"       },

    //Facilities & Furniture
    { "AC",  "Air Conditioner"  },
    { "LMP", "Lampu"            },
    { "LMR", "Lemari"           },
    { "MJA", "Meja"             },
    { "KRS", "Kursi"            },
    { "SF",  "Sofa"             },
    { "WBD", "Whiteboard"       },
    { "TPD", "Tripod"           },
    { "BRK", "Brankas"          },
    { "DSN", "Dispenser"        },
    { "DSP", "Dispenser"    },
    { "TND", "Tenda"            },
    { "PAL", "Palet"            },
    { "BIN", "Tempat Sampah"    },
    { "PLT", "Piring"           },
    { "VSE", "Vas Bunga"        },
    { "CUP", "Gelas Kertas"     },
    { "BWL", "Mangkuk"          },
    { "FRK", "Garpu"            },
    { "SPN", "Sendok"           },
    { "KNF", "Pisau"            },
    { "MUG", "Cangkir"          },
    { "GLS", "Gelas Kaca"       },
    { "TOW", "Handuk"           },
    { "SP",  "Spons"            },
    { "MP",  "Alat Pel"         },
    { "BRM", "Sapu"             },
    { "DST", "Kemoceng"         },
    { "VAC", "Vacuum Cleaner"   },

    //Tools & Workshop
    { "SLD", "Solder"           },
    { "MLT", "Multimeter"       },
    { "TNG", "Tang"             },
    { "TST", "Tester"           },
    { "OBN", "Obeng"            },
    { "PLR", "Tang Potong"      },
    { "WRN", "Kunci Inggris"    },
    { "LVL", "Waterpass"        },
    { "TAP", "Meteran"          },
    { "HMR", "Palu"             },
    { "SAW", "Gergaji"          },
    { "DRL", "Bor Listrik"      },
    { "GLG", "Gerinda Listrik"  },
    { "BTL", "botol"},

    //Office Supplies 
    { "STP", "Stapler"          },
    { "CLP", "Klip Kertas"      },
    { "CTR", "Cutter"           },
    { "ICT", "Isi Cutter"       },
    { "SPD", "Spidol"           },
    { "PHP", "Handphone"        },
    { "P3K", "Kotak P3K"        },
    { "BND", "Perban"           },
    { "UU",  "Dokumen UU"       },
    { "ATR", "Aturan Kerja"     },
    { "GNT", "Gunting"          },
    { "STF", "Stopmap Folio"    },
    { "IST", "Isolasi Transparan"},
    { "KA4", "Kertas A4"        },
    { "KF4", "Kertas F4"        },
    { "NT",  "Notebook"         },
    { "MAP", "Map Folder"       },
    { "BKS", "Buku Tulis"       },
    { "LBN", "Lakban"           },
};


static const dict_entry_t PROGMEM cat_dict[] =
{
    { "ATK", "Alat Tulis Kantor" },
    { "ELK", "Elektronik"        },
    { "FAS", "Fasilitas"         },
    { "PRK", "Perkakas"          },
    { "KES", "Kesehatan"         },
    { "SAN", "Sanitasi"          },
    { "DOK", "Dokumen"           },
    { "ATR", "Aturan/Regulasi"   },
};


static const dict_entry_t PROGMEM loc_dict[] =
{
    /* Lantai / Ruang */
    { "L1",  "Lantai 1"          },
    { "L2",  "Lantai 2"          },
    { "R1",  "Ruang 1"           },
    { "R2",  "Ruang 2"           },
    { "R3",  "Ruang 3"           },
    { "R4",  "Ruang 4"           },
    /* Area Khusus */
    { "G1",  "Gudang 1"          },
    { "G2",  "Gudang 2"          },
    { "SR",  "Server Room"       },
    { "WS",  "Workshop"          },
    { "ST",  "Studio"            },
    { "LB",  "Lobby"             },
    { "M1",  "Meeting Room 1"    },
    { "M2",  "Meeting Room 2"    },
    { "PM",  "Pos Masuk"         },
    { "PT",  "Pantry"            },
    { "RA",  "Rak A"             },
    { "RF",  "Rak F"             },
    { "PP",  "Pos Penjagaan"     },
    { "KL",  "Klinik"            },
    /* Maping nama panjang yang sudah ada di EEPROM */
    { "A1",  "Cabinet A Row 1"   },
    { "A2",  "Cabinet A Row 2"   },
    { "B1",  "Cabinet B Row 1"   },
};


static const dict_entry_t PROGMEM owner_dict[] =
{
    { "GA",  "General Affairs"   },
    { "IT",  "IT Department"     },
    { "HR",  "Human Resources"   },
    { "FN",  "Finance"           },
    { "AD",  "Administrasi"      },
    { "LG",  "Logistik"          },
    { "MK",  "Marketing"         },
    { "CR",  "Creative"          },
    { "PR",  "Procurement"       },
    { "OP",  "Operasional"       },
};

static const dict_entry_t PROGMEM pic_dict[] =
{
    { "BD",  "Budi"              },
    { "RN",  "Rini"              },
    { "ST",  "Siti"              },
    { "AG",  "Agus"              },
    { "AN",  "Andi"              },
    { "DD",  "Dedi"              },
    { "TK",  "Tika"              },
    { "JN",  "Jono"              },
    { "ED",  "Edi"               },
    { "MG",  "Margo"             },
    { "SN",  "Seno"              },
};

/* -----------------------------------------------------------------------
 * Generic lookup: searches a PROGMEM table, copies full name to out.
 * Falls back to printing the raw code if no match is found.
 * ---------------------------------------------------------------------- */
static void dict_lookup(const dict_entry_t *table,
                        uint8_t            table_len,
                        const char        *code,
                        char              *out)
{
    for (uint8_t i = 0; i < table_len; i++)
    {
        /* Read the code field from Flash into a temporary SRAM buffer */
        char key[DICT_CODE_SIZE];
        memcpy_P(key, table[i].code, DICT_CODE_SIZE);

        if (strncmp(key, code, DICT_CODE_SIZE) == 0)
        {
            /* Match found: copy full name from Flash to output buffer */
            memcpy_P(out, table[i].full, DICT_FULL_SIZE);
            out[DICT_FULL_SIZE - 1] = '\0';
            return;
        }
    }

    /* No match: fall back to raw code (safe, always null-terminated) */
    strncpy(out, code, DICT_FULL_SIZE - 1);
    out[DICT_FULL_SIZE - 1] = '\0';
}

/* -----------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
void dict_lookup_name(const char *code, char *out)
{
    dict_lookup(name_dict,
                (uint8_t)(sizeof(name_dict) / sizeof(name_dict[0])),
                code, out);
}

void dict_lookup_cat(const char *code, char *out)
{
    dict_lookup(cat_dict,
                (uint8_t)(sizeof(cat_dict) / sizeof(cat_dict[0])),
                code, out);
}

void dict_lookup_loc(const char *code, char *out)
{
    dict_lookup(loc_dict,
                (uint8_t)(sizeof(loc_dict) / sizeof(loc_dict[0])),
                code, out);
}

void dict_lookup_owner(const char *code, char *out)
{
    dict_lookup(owner_dict,
                (uint8_t)(sizeof(owner_dict) / sizeof(owner_dict[0])),
                code, out);
}

void dict_lookup_pic(const char *code, char *out)
{
    dict_lookup(pic_dict,
                (uint8_t)(sizeof(pic_dict) / sizeof(pic_dict[0])),
                code, out);
}
