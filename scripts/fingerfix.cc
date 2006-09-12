#include "nbase.h"
#include "nmap.h"
#include "osscan.h"
#include "MACLookup.h"

// attribute value length
#define AVLEN 128

void usage() {
  printf("Usage: fingerdiff\n"
         "(You will be prompted for the fingerprint data)\n\n");

  exit(1);
}

static int checkFP(char *FP) {
  char *p;
  char macbuf[16];
  u8 macprefix[3];
  char tmp;
  bool founderr = false;
  int i;

  // SCAN
  p = strstr(FP, "SCAN(");
  if(!p) {
	founderr = true;
	printf("[WARN] SCAN line is missing");
  } else {
	// SCAN.G: whether the fingerprint is good
	p = strstr(FP, "%G=");
	if(!p) p = strstr(FP, "(G=");
	if(!p) {
	  printf("[WARN] Attribute G is missing in SCAN line\n");
	  founderr = true;
	} else {
	  tmp = *(p+3);
	  if(tmp != 'Y') {
		printf("[WARN] One fingerprint is not good\n");
		founderr = true;
	  }
	}
	
	// SCAN.M: mac prefix of the target.
	// if there is a MAC prefix, print the vendor name
	p = strstr(FP, "%M=");
	if(!p) p = strstr(FP, "(M=");
	if(p) {
	  p = p + 3;
	  for(i = 0; i < 6; i++) {
		if(!p[i] || !isxdigit(p[i])) {
		  printf("[WARN] Invalid value (%s) occurs in SCAN.M\n", p);
		  founderr = true;
		  break;
		}
	  }
	  if(!founderr) {
		strncpy(macbuf, p, 6);
		i = strtol(macbuf, NULL, 16);
		macprefix[0] = i >> 16;
		macprefix[1] = (i >> 8) & 0xFF;
		macprefix[2] = i & 0xFF;
		printf("[INFO] Vendor Info: %s\n", MACPrefix2Corp(macprefix));
	  }
	}
  }

  /* Now we validate that all elements are present */
  p = FP;
  if (!strstr(p, "SEQ(") || !strstr(p, "OPS(") || !strstr(p, "WIN(") || 
	  !strstr(p, "ECN(") || !strstr(p, "T1(") || !strstr(p, "T2(") || 
      !strstr(p, "T3(") || !strstr(p, "T4(") || !strstr(p, "T5(") || 
      !strstr(p, "T6(") || !strstr(p, "T7(") || !strstr(p, "U1(") ||
	  !strstr(p, "IE(")) {
    /* This ought to get my attention :) */
	founderr = true;
    printf("[WARN] Fingerprint is missing at least 1 element\n");
  }
  
  if(founderr) return -1;
  return 0;
}

/* Returns -1 (or exits) for failure */
static int readFP(FILE *filep, char *FP, int FPsz ) {
  char line[512];
  int linelen = 0;
  int lineno = 0;
  char *p, *q;
  char *oneFP;
  char *dst = FP;
  char tmp[16];
  int i;
  bool isInWrappedFP = false; // whether we are currently reading in a
							  // wrapped fingerprint
  
  if(FPsz < 50) return -1;
  FP[0] = '\0';

  while((fgets(line, sizeof(line), filep))) {
	lineno++;
	linelen = strlen(line);
	p = line;
    if (*p == '\n' || *p == '.') {
	  // end of input

	  if(isInWrappedFP) {
		// We have just completed reading in a wrapped fp. Because a
		// wrapped fp is submitted by user, so we check if there is a
		// SCAN line in it. If yes, look inside the scan line.
		*dst = '\0';
		checkFP(oneFP);
		isInWrappedFP = false;
	  }	  
	  break;
	}
	while(*p && isspace(*p)) p++;
    if (*p == '#') 
      continue; // skip the comment line

    if (dst - FP + linelen >= FPsz - 5)
      fatal("[ERRO] Overflow!\n");
	
	if(strncmp(p, "OS:", 3) == 0) {
	  // the line is start with "OS:"
	  if(!isInWrappedFP) {
		// just enter a wrapped fp area
		oneFP = dst;
		isInWrappedFP = true;
	  }
	  p += 3;
	  while(*p != '\r' && *p != '\n') {
		*dst++ = toupper(*p);
		if(*p == ')') *dst++ = '\n';
		p++;
	  }
	  continue;
	}

	// this line is not start with "OS:"
	if(isInWrappedFP) {
	  // We have just completed reading in a wrapped fp. Because a
	  // wrapped fp is submitted by user, so we check if there is a
	  // SCAN line in it. If yes, look inside the scan line.
	  *dst = '\0';
	  checkFP(oneFP);
	  isInWrappedFP = false;
	}

	q = p; i = 0;
	while(q && *q && i<12)
	  tmp[i++] = toupper(*q++);
	tmp[i] = '\0';
	if(strncmp(tmp, "FINGERPRINT", 11) == 0) {
	  q = p + 11;
	  while(*q && isspace(*q)) q++;
	  if (*q) { // this fingeprint line is not empty
		strncpy(dst, "Fingerprint", 11);
		dst += 11;
		p += 11;
		while(*p) *dst++ = *p++;
	  }
	  continue;
	} else if(strncmp(tmp, "CLASS", 5) == 0) {
	  q = p + 5;
	  while(*q && isspace(*q)) q++;
	  if (*q) {// this class line is not empty
		strncpy(dst, "Class", 5);
		dst += 5;
		p += 5;
		while(*p) *dst++ = *p++;
	  }
	  continue;
	} else if(strchr(p, '(')) {
	  while(*p) *dst++ = toupper(*p++);
	} else {
	  printf("[WARN] Skip bogus line: %s\n", p);
	  continue;
	}
  }

  // Now we validate that all elements are present. Though this maybe
  // redundant because we have checked it for those wrapped FPs, it
  // doesn't hurt to give a duplicated warning here.
  p = FP;
  if (!strstr(p, "SEQ(") || !strstr(p, "OPS(") || !strstr(p, "WIN(") || 
	  !strstr(p, "ECN(") || !strstr(p, "T1(") || !strstr(p, "T2(") || 
      !strstr(p, "T3(") || !strstr(p, "T4(") || !strstr(p, "T5(") || 
      !strstr(p, "T6(") || !strstr(p, "T7(") || !strstr(p, "U1(") ||
	  !strstr(p, "IE(")) {
    /* This ought to get my attention :) */
    printf("[WARN] Fingerprint is missing at least 1 element\n");
  }
  
  if (dst - FP < 1)
    return -1;
  return 0;
}

typedef enum {
  STR, DECNUM, HEXNUM
} SortAs;

static void sort_and_merge(struct AVal *result, char values[][AVLEN], int num, SortAs sortas) {
  // sort and merge input values like 4,12,8 to 4|8|12. The sorting is
  // firstly based on their length, and secondly based on their ascii
  // order.
  assert(num > 0);
  
  int i, j;
  char tmp[AVLEN];
  int offset;
  bool lt;
  unsigned int val1, val2;
  int base;
  
  // sort
  for(i = 0; i < num; i++) {
	for(j = 1; j < num - i; j++) {
	  lt = false;
	  if(sortas == STR) {
		// sort as string
		// string with less length is regarded as "smaller"
		if((strlen(values[j-1]) > strlen(values[j])) ||
		   (strlen(values[j-1]) == strlen(values[j]) && strcmp(values[j-1], values[j]) > 0)) {
		  lt = true;
		}
	  } else {
		// sort as number
		if(sortas == DECNUM) base = 10;
		else if(sortas == HEXNUM) base = 16;
		val1 = strtol(values[j-1], NULL, base);
		val2 = strtol(values[j], NULL, base);
		if(val1 > val2) lt = true;
	  }

	  if(lt) {
		// printf("swap %s and %s\n", values[j-1], values[j]);
		strcpy(tmp, values[j-1]);
		strcpy(values[j-1], values[j]);
		strcpy(values[j], tmp);
	  }
	}
  }

  // merge.
  offset = 0;
  for(i = 0; i < num; i++) {
	if(i > 0 && strcmp(values[i], tmp) == 0) {
	  // this is a duplicated value;
	  continue;
	}
	strcpy(tmp, values[i]);
	offset += snprintf(result->value + offset, AVLEN - offset, "%s|", values[i]);
	if(offset >= AVLEN) {
	  printf("[WARN] Attribute %s is too long and has been truncated\n", result->attribute);
	  return;
	}
  }
  result->value[offset-1] = '\0'; // remove the final '|'
}

static void merge_gcd(struct AVal *result, char values[][AVLEN], int num) {
  // When fingerfix gets a GCD like 0x32, it changes it to "GCD=<67".
  // That seems kindof bogus.  The GCD is only likely to be 0x32 or
  // (in rare cases by coincidence) a small multiple of that.  So I
  // think it should give "GCD=32|64|96|C8|FA|12C".  For the common
  // case of GCD=1, still saying GCD=<7 is desirable because that is
  // shorter than GCD=1|2|3|4|5|6 .
  assert(num > 0);

  const unsigned int LIM = 7;
  int i, j;
  char *p, *q, *endptr;
  unsigned int val;
  unsigned int curlim = 0;
  bool haslim = false;
  int newValueNum = 0;
  char newValues[128][AVLEN];
  
  // first let's find the limit
  for(i = 0; i < num; i++) {
	p = values[i];
	q = strchr(p, '<');
	if(q) {
	  val = strtol(q + 1, &endptr, 16);
	  if (q != p || *endptr) {
		printf("[WARN] Invalid value (%s) occurs in attribute SEQ.GCD\n", p);
		continue;
	  }
	  if(curlim < val) {
		curlim = val;
		haslim = true;
	  }
	}
  }

  // Normally the limit should be the same with the LIM, since any limit
  // exist in the fingerprint is supposed to be generated by this
  // tool. Thus if it is not the case, print a warning.
  if(haslim && curlim != LIM) {
	printf("[WARN] Odd limit (%X) occurs in attribute SEQ.GCD", curlim);
	if(curlim < LIM) curlim = LIM;
  }
  
  for(i = 0; i < num; i++) {
	p = values[i];
	q = strchr(p, '<');
	if(q) continue; // let's skip those limit strings
	val = strtol(p, &endptr, 16);
	if (*endptr) {
	  printf("[WARN] Invalid value (%s) occurs in attribute SEQ.GCD\n", p);
	  continue;
	} else if(val == 0) {
	  printf("[WARN] Zero value occurs in attribute SEQ.GCD\n");
	}
	
	if(haslim) {
	  if(val <= curlim) continue;
	} else if(val <= LIM) {
	  curlim = LIM;
	  haslim = true;
	  continue;
	}

	for(j = 1; j < 7; j++)
	  snprintf(newValues[newValueNum++], AVLEN, "%X", val * j);
  }

  if(newValueNum == 0 && haslim) {
	snprintf(result->value, AVLEN, "<%X", curlim);
  } else if(newValueNum > 0 && !haslim) {
	sort_and_merge(result, newValues, newValueNum, HEXNUM);
  } else if(newValues > 0 && haslim) {
	struct AVal semiresult;
	int offset;
	semiresult.attribute = "GCD";
	sort_and_merge(&semiresult, newValues, newValueNum, HEXNUM);
	// insert the limit string to the front of it
	offset = snprintf(result->value, AVLEN, "<%X|%s", curlim, semiresult.value);
	if(offset>=AVLEN) {
	  printf("[WARN] SEQ.GCD is too long and has been truncated\n");
	}
  } else {
	result->value[0] = '\0';
  }
}

static void merge_sp_or_isr(struct AVal *result, char values[][AVLEN], int num) {
  // Fingerfix should expand elements based on observed deviation.
  // So if a fingerprint comes in with SP=0x9C (and that is the only
  // SEQ line), that means that all 3 Nmap tries had SP=0x9C and so it
  // may be OK to just use 0x9C in the reference fingerprint
  // generated, or maybe expand it by 1 like 0x9B-0x9D.  But if the
  // fingerprint has 3 seq lines (which Nmap does when any elements in
  // it differ) and they show SP values of 0x9C, 0x84, and 0xA7, you
  // may want to handle them as: 1) You see 0x9C.  Now your low and
  // high values are 0x9c 2) You see 0x84.  This makes for a low of
  // 0x84 and a high of 0x9C you've seen.  But you may want to double
  // the size of this new range, so it is now 0x78-0xA8.  Note that
  // since your previous low and high values were equal, you expand in
  // both directions rather than just expanding in one direction.  But
  // once you have developed a range, you'll start expanding only in
  // the direction of the outlying value.  3) You see 0xA7.  This fits
  // into your range so you ignore it.  4) Suppose you then see 0xAA.
  // This is 3 higher than the top of your range, so you double that
  // and extend your upper range by 6 to 0x78-0xAD.  It is important
  // to be careful not to keep expanding already-expanded values.  So
  // if someone passes 0x78-0xAD in a reference FP to fingerfix, don't
  // double that range again.  But if you then see 0x77, you would
  // change your range to 0x76-0xAD. This way we keep tight
  // fingerprints where the SP doesn't differ so much, but we avoid
  // having to constantly tweak fingerprints which differ
  // dramatically.
  assert(num > 0);
  int i;
  int low, high, val1, val2;
  char *p, *q, *r, buf[AVLEN];

  result->value[0] = '\0';
  
  for(i = 0; i < num; i++) {
	strncpy(buf, values[i], AVLEN);
	p = strchr(buf, '-');

	if(p) {
	  // an interval
	  *p = '\0';
	  val1 = (int)strtol(buf, &q, 16);
	  val2 = (int)strtol(p+1, &r, 16);
	  if(*q || *r || val1 >= val2) {
		printf("[WARN] Invalid value (%s) occurs in attribute SEQ.%s\n", values[i], result->attribute);
		return;
	  }

	  // do not expand an interval
	  if(i == 0) {
		low = val1;
		high = val2;
	  } else {
		if(val1 < low) low = val1;
		if(val2 > high) high = val2;
	  }
	} else {
	  // a value
	  val1 = (int)strtol(buf, &p, 16);
	  if(*p) {
		printf("[WARN] Invalid value (%s) occurs in attribute SEQ.%s\n", values[i], result->attribute);
		return;
	  }
	  if(!val1) {
		// a zero sp/isr value, this should
		printf("[WARN] Zero value occurs in attribute SEQ.%s. A constant ISN sequence?\n", result->attribute);
	  }
	  if(i == 0) {
		low = high = val1;
	  } else {
		if(low == high && val1 != low) {
		  // expand it in both directions
		  if(val1 < low) {
			low = val1 - (low - val1) / 2;
			high = high + (high - val1) / 2;
		  } else {
			low = low - (val1 - low) / 2;
			high = high + (val1 - high) / 2;
		  }
		  if(low < 0) low = 0;
		} else if(val1 < low) {
		  // expand in the left direction
		  low = val1 - (low - val1);
		  if(low < 0) low = 0;		  
		} else if(val1 > high) {
		  // expand in the right direction
		  high = val1 + (val1 - high);
		}
	  }
	}
  }

  if(low == high && low == 0) {
	snprintf(result->value, AVLEN, "0");
	return;
  }

  if(low == high) {
	// expanded it a little
	low = low - 1;
	if(low < 0) low = 0;
	high = high + 1;
  }

  snprintf(result->value, AVLEN, "%X-%X", low, high);
}

int main(int argc, char *argv[]) {
  FingerPrint *observedFP;
  FingerPrint *resultFP;
  FingerPrint *resultFPLine, *observedFPLine;
  char observedFPString[10240];
  char resultTemplate[] = {"SEQ(SP=%GCD=%ISR=%TI=%II=%SS=%TS=)\n"
						   "OPS(O1=%O2=%O3=%O4=%O5=%O6=)\n"
						   "WIN(W1=%W2=%W3=%W4=%W5=%W6=)\n"
						   "ECN(R=%DF=%T=%TG=%W=%O=%CC=%Q=)\n"
						   "T1(R=%DF=%T=%TG=%S=%A=%F=%RD=%Q=)\n"
						   "T2(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "T3(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "T4(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "T5(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "T6(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "T7(R=%DF=%T=%TG=%W=%S=%A=%F=%O=%RD=%Q=)\n"
						   "U1(DF=%T=%TG=%TOS=%IPL=%UN=%RIPL=%RID=%RIPCK=%RUCK=%RUL=%RUD=)\n"
						   "IE(DFI=%T=%TG=%TOSI=%CD=%SI=%DLI=)\n"
  };

  if (argc > 1)
    usage();

  observedFPString[0] = '\0';
  printf("Enter the fingerprint(s) you want to fix, followed by a blank or single-dot line:\n");

  if (readFP(stdin, observedFPString, sizeof(observedFPString)) == -1)
    fatal("[ERRO] Failed to read in supposed fingerprint from stdin\n");

  observedFP = parse_single_fingerprint(observedFPString);
  if (!observedFP) fatal("[ERRO] failed to parse the observed fingerprint you entered\n");
  // printf("%s", fp2ascii(observedFP));
  
  resultFP = parse_single_fingerprint(resultTemplate);
  // printf("%s", fp2ascii(resultFP));

  bool foundline;
  struct AVal *resultAV, *observedAV, *tmpAV;
  char values[16][AVLEN];
  int avnum;
  int i;

  for(resultFPLine = resultFP; resultFPLine; resultFPLine = resultFPLine->next) {
	// step 1:
	//
	// Check if this line is missing in the input fingerprint. If yes,
	// replace the result line with a R=N.
	foundline = false;
	for(observedFPLine = observedFP; observedFPLine; observedFPLine = observedFPLine->next) {
	  if(observedFPLine->name && strcmp(resultFPLine->name, observedFPLine->name) == 0) {
		// Found the corresponding line. If it doesn't begin with a
		// R=N, then we take it as a good line.
		if(observedFPLine->results &&
		   !(strcmp(observedFPLine->results->attribute, "R") == 0 &&
			 strcmp(observedFPLine->results->value, "N") == 0)) {
		  foundline = true;
		  break;
		}
	  }
	}

	if(!foundline) {
	  // replace the fingerprint line with a R=N
	  free(resultFPLine->results);		
	  tmpAV = (struct AVal *) safe_zalloc(sizeof(struct AVal));
	  tmpAV->attribute = "R";
	  strcpy(tmpAV->value, "N");
	  resultFPLine->results = tmpAV;
	  continue;
	}
	
	// step 2:
	//
	// For each AVal of this fingerprint line, merge all the
	// counterpart values appeared in the input fingerprint.
	for(resultAV = resultFPLine->results; resultAV; resultAV = resultAV->next) {
	  avnum = 0;
	  for(observedFPLine = observedFP; observedFPLine; observedFPLine = observedFPLine->next) {
		if(strcmp(resultFPLine->name, observedFPLine->name) == 0) {
		  for(observedAV = observedFPLine->results; observedAV; observedAV = observedAV->next) {
			if(strcmp(resultAV->attribute, observedAV->attribute) == 0) {
			  // check if we have stored the same attribute value if
			  // not, store it
			  bool stored;
			  char *p, *q;
			  p = observedAV->value;
			  while(p && *p) {
				stored = false;
				q = strchr(p, '|');
				if(q) *q = '\0';
				for(i = 0; i < avnum; i++) {
				  if(strcmp(values[i], p) == 0) {
					stored = true;
					break;
				  }
				}
				if(!stored) {
				  strcpy(values[avnum++], p);
				}
				if(q) p = q + 1;
				else break;
			  }
			}
		  }
		}
	  }

	  if(avnum == 0) {
	   // no value for this attribute, now handle the next attribute
		continue;
	  }

	  // now we get all the values for this attribute, it's time to
	  // merge them. let's first handle some special attributes.
	  if(strcmp(resultFPLine->name, "SEQ") == 0 && strcmp(resultAV->attribute, "GCD") == 0) {
		// SEQ.GCD
		merge_gcd(resultAV, values, avnum);
	  } else if(strcmp(resultFPLine->name, "SEQ") == 0 &&
				(strcmp(resultAV->attribute, "SP") == 0 || strcmp(resultAV->attribute, "ISR") == 0)) {
		// SEQ.SP or SEQ.ISR
		merge_sp_or_isr(resultAV, values, avnum);
	  } else {
		// common merge
		sort_and_merge(resultAV, values, avnum, STR);
	  }
	}

	// step 3:
	// handle some special cases:
	// o remove SEQ.SS if it is null
	// o make up the TTL and TTL guess stuff
	struct AVal *av1, *av2;

	// remove SEQ.SS
	if(strcmp(resultFPLine->name, "SEQ") == 0) {
	  av1 = resultFPLine->results;
	  while(av1) {
		if(strcmp(av1->attribute, "SS") == 0 && strlen(av1->value) == 0) {
		  if(av1 == resultFPLine->results) {
			resultFPLine->results = av1->next;
			break;
		  } else {
			av2->next = av1->next;
			break;
		  }
		}
		av2 = av1;
		av1 = av1->next;
	  }
	}
	
	// TTL and TTL Guess
	av1 = NULL;
	av2 = NULL;
	if(strcmp(resultFPLine->name, "ECN") == 0 ||
	   strcmp(resultFPLine->name, "T1") == 0 ||
	   strcmp(resultFPLine->name, "T2") == 0 ||
	   strcmp(resultFPLine->name, "T3") == 0 ||
	   strcmp(resultFPLine->name, "T4") == 0 ||
	   strcmp(resultFPLine->name, "T5") == 0 ||
	   strcmp(resultFPLine->name, "T6") == 0 ||
	   strcmp(resultFPLine->name, "T7") == 0 ||
	   strcmp(resultFPLine->name, "U1") == 0 ||
	   strcmp(resultFPLine->name, "IE") == 0) {
	  for(tmpAV = resultFPLine->results; tmpAV; tmpAV = tmpAV->next ) {
		if(strcmp(tmpAV->attribute, "T") == 0) {
		  av1 = tmpAV;
		} else if(strcmp(tmpAV->attribute, "TG") == 0) {
		  av2 = tmpAV;
		}
	  }

	  assert(av1&&av2);
	  if(strlen(av1->value) == 0 && strlen(av2->value) > 0) {
		strcpy(av1->value, av2->value);
	  } else if(strlen(av2->value) == 0 && strlen(av1->value) > 0) {
		strcpy(av2->value, av1->value);
	  }
	}
  }

  // whew, we finally complete the job, now spit it out.
  printf("\nADJUSTED FINGERPRINT:\n");

  // OS Name
  if(observedFP->OS_name) {
	printf("Fingerprint %s\n", observedFP->OS_name);
  } else {
	// print an empty fingerprint
	printf("Fingerprint\n");
  }

  // Class
  if(observedFP->num_OS_Classifications > 0) {
	for(i = 0; i<observedFP->num_OS_Classifications; i++) {
	  printf("Class %s | %s |",
			 observedFP->OS_class[i].OS_Vendor,
			 observedFP->OS_class[i].OS_Family);
	  if(observedFP->OS_class[i].OS_Generation) {
		printf(" %s |", observedFP->OS_class[i].OS_Generation);
	  } else {
		printf("|");
	  }
	  printf(" %s\n", observedFP->OS_class[i].Device_Type);
	}
  } else {
	// print a empty class line
	printf("Class\n");
  }

  // Fingerprint
  printf("%s", fp2ascii(resultFP));
  
  return 0;
}
