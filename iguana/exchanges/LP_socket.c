/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

/**
 * - we need to include WinSock2.h header to correctly use windows structure
 * as the application is still using 32bit structure from mingw so, we need to
 * add the include based on checking
 * @author - fadedreamz@gmail.com
 * @remarks - #if (defined(_M_X64) || defined(__amd64__)) && defined(WIN32)
 *     is equivalent to #if defined(_M_X64) as _M_X64 is defined for MSVC only
 */
#if defined(_M_X64)
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#endif

int32_t LP_socket(int32_t bindflag,char *hostname,uint16_t port)
{
    int32_t opt,sock,result; char ipaddr[64],checkipaddr[64]; struct timeval timeout;
    struct sockaddr_in saddr; socklen_t addrlen,slen;
    addrlen = sizeof(saddr);
    struct hostent *hostent;
    
    /**
     * gethostbyname() is deprecated and cause crash on x64 windows
     * the solution is to implement similar functionality by using getaddrinfo()
     * it is standard posix function and is correctly supported in win32/win64/linux
     * @author - fadedreamz@gmail.com
     */
#if defined(_M_X64)
    struct addrinfo *addrresult = NULL;
    struct addrinfo *returnptr = NULL;
    struct addrinfo hints;
    struct sockaddr_in * sockaddr_ipv4;
    int retVal;
    int found = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
#endif
    
    if ( parse_ipaddr(ipaddr,hostname) != 0 )
        port = parse_ipaddr(ipaddr,hostname);
    
#if defined(_M_X64)
    retVal = getaddrinfo(ipaddr, NULL, &hints, &addrresult);
    for (returnptr = addrresult; returnptr != NULL && found == 0; returnptr = returnptr->ai_next) {
        switch (returnptr->ai_family) {
            case AF_INET:
                sockaddr_ipv4 = (struct sockaddr_in *) returnptr->ai_addr;
                // we want to break from the loop after founding the first ipv4 address
                found = 1;
                break;
        }
    }
    
    // if we iterate through the loop and didn't find anything,
    // that means we failed in the dns lookup
    if (found == 0) {
        printf("getaddrinfo(%s) returned error\n", hostname);
        freeaddrinfo(addrresult);
        return(-1);
    }
#else
    hostent = gethostbyname(ipaddr);
    if ( hostent == NULL )
    {
        printf("gethostbyname(%s) returned error: %d port.%d ipaddr.(%s)\n",hostname,errno,port,ipaddr);
        return(-1);
    }
#endif
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    //#ifdef _WIN32
    //   saddr.sin_addr.s_addr = (uint32_t)calc_ipbits("127.0.0.1");
    //#else
    
#if defined(_M_X64)
    saddr.sin_addr.s_addr = sockaddr_ipv4->sin_addr.s_addr;
    // graceful cleanup
    sockaddr_ipv4 = NULL;
    freeaddrinfo(addrresult);
#else
    memcpy(&saddr.sin_addr.s_addr,hostent->h_addr_list[0],hostent->h_length);
#endif
    expand_ipbits(checkipaddr,saddr.sin_addr.s_addr);
    if ( strcmp(ipaddr,checkipaddr) != 0 )
        printf("bindflag.%d iguana_socket mismatch (%s) -> (%s)?\n",bindflag,checkipaddr,ipaddr);
    //#endif
    if ( (sock= socket(AF_INET,SOCK_STREAM,0)) < 0 )
    {
        if ( errno != ETIMEDOUT )
            printf("socket() failed: %s errno.%d", strerror(errno),errno);
        return(-1);
    }
    opt = 1;
    slen = sizeof(opt);
    //printf("set keepalive.%d\n",setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,(void *)&opt,slen));
#ifndef _WIN32
    if ( 1 )//&& bindflag != 0 )
    {
        opt = 0;
        getsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,(void *)&opt,&slen);
        opt = 1;
        //printf("keepalive.%d\n",opt);
    }
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(void *)&opt,sizeof(opt));
#ifdef __APPLE__
    setsockopt(sock,SOL_SOCKET,SO_NOSIGPIPE,&opt,sizeof(opt));
#endif
#endif
    if ( bindflag == 0 )
    {
        printf("call connect sock.%d\n",sock);
        result = connect(sock,(struct sockaddr *)&saddr,addrlen);
        printf("called connect result.%d\n",result);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(void *)&timeout,sizeof(timeout));
        if ( result != 0 )
        {
            if ( errno != ECONNRESET && errno != ENOTCONN && errno != ECONNREFUSED && errno != ETIMEDOUT && errno != EHOSTUNREACH )
            {
                //printf("%s(%s) port.%d failed: %s sock.%d. errno.%d\n",bindflag!=0?"bind":"connect",hostname,port,strerror(errno),sock,errno);
            }
            if ( sock >= 0 )
                closesocket(sock);
            return(-1);
        }
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(void *)&timeout,sizeof(timeout));
    }
    else
    {
        while ( (result= bind(sock,(struct sockaddr*)&saddr,addrlen)) != 0 )
        {
            if ( errno == EADDRINUSE )
            {
                sleep(1);
                printf("ERROR BINDING PORT.%d. this is normal tcp timeout, unless another process is using port\n",port);
                fflush(stdout);
                sleep(3);
                printf("%s(%s) port.%d try again: %s sock.%d. errno.%d\n",bindflag!=0?"bind":"connect",hostname,port,strerror(errno),sock,errno);
                if ( bindflag == 1 )
                {
                    closesocket(sock);
                    return(-1);
                }
                sleep(13);
                //continue;
            }
            if ( errno != ECONNRESET && errno != ENOTCONN && errno != ECONNREFUSED && errno != ETIMEDOUT && errno != EHOSTUNREACH )
            {
                printf("%s(%s) port.%d failed: %s sock.%d. errno.%d\n",bindflag!=0?"bind":"connect",hostname,port,strerror(errno),sock,errno);
                closesocket(sock);
                return(-1);
            }
        }
        if ( listen(sock,64) != 0 )
        {
            printf("listen(%s) port.%d failed: %s sock.%d. errno.%d\n",hostname,port,strerror(errno),sock,errno);
            if ( sock >= 0 )
                closesocket(sock);
            return(-1);
        }
    }
#ifdef __APPLE__
    //timeout.tv_sec = 0;
    //timeout.tv_usec = 30000;
    //setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(void *)&timeout,sizeof(timeout));
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(void *)&timeout,sizeof(timeout));
#endif
    return(sock);
}

int32_t LP_socketsend(int32_t sock,uint8_t *serialized,int32_t len)
{
    int32_t numsent,remains,flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL;
#endif
    remains = len;
    while ( sock >= 0 && remains > 0 )
    {
        if ( (numsent= (int32_t)send(sock,serialized,remains,flags)) < 0 )
        {
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                sleep(1);
                continue;
            }
            printf("(%s): numsent.%d vs remains.%d len.%d errno.%d (%s) usock.%d\n",serialized,numsent,remains,len,errno,strerror(errno),sock);
            return(-errno);
        }
        else if ( remains > 0 )
        {
            remains -= numsent;
            serialized += numsent;
            if ( remains > 0 )
                printf("%d LP_socket sent.%d remains.%d of len.%d\n",sock,numsent,remains,len);
        }
        //printf("numsent.%d vs remains.%d len.%d sock.%d\n",numsent,remains,len,sock);
    }
    return(len);
}

int32_t LP_socketrecv(int32_t sock,uint8_t *recvbuf,int32_t maxlen)
{
    int32_t recvlen = -1;
    while ( sock >= 0 )
    {
        if ( (recvlen= (int32_t)recv(sock,recvbuf,maxlen,0)) < 0 )
        {
            if ( errno == EAGAIN )
            {
                //printf("%s recv errno.%d %s len.%d remains.%d\n",ipaddr,errno,strerror(errno),len,remains);
                //printf("EAGAIN for len %d, remains.%d\n",len,remains);
                sleep(1);
            } else return(-errno);
        } else break;
    }
    return(recvlen);
}

struct electrum_info
{
    queue_t sendQ,pendingQ;
    portable_mutex_t mutex,txmutex;
    struct electrum_info *prev;
    int32_t bufsize,sock,*heightp,numerrors;
    struct iguana_info *coin;
    uint32_t stratumid,lasttime,keepalive,pending,*heighttimep;
    char ipaddr[64],symbol[16];
    uint16_t port;
    uint8_t buf[];
} *Electrums[8192];
int32_t Num_electrums;

struct electrum_info *electrum_server(char *symbol,struct electrum_info *ep)
{
    struct electrum_info *rbuf[128],*recent_ep; uint32_t recent,mostrecent = 0; int32_t i,n = 0;
    portable_mutex_lock(&LP_electrummutex);
    if ( ep == 0 )
    {
        //printf("find random electrum.%s from %d\n",symbol,Num_electrums);
        memset(rbuf,0,sizeof(rbuf));
        recent_ep = 0;
        recent = (uint32_t)time(NULL) - 300;
        for (i=0; i<Num_electrums; i++)
        {
            ep = Electrums[i];
            if ( strcmp(symbol,ep->symbol) == 0 && ep->sock >= 0 )
            {
                if ( ep->lasttime > recent )
                {
                    rbuf[n++] = ep;
                    if ( n == sizeof(rbuf)/sizeof(*rbuf) )
                        break;
                }
                else if ( ep->lasttime > mostrecent )
                {
                    mostrecent = ep->lasttime;
                    recent_ep = ep;
                }
            }
        }
        ep = recent_ep;
        if ( n > 0 )
        {
            i = (rand() % n);
            ep = rbuf[i];
        }
    }
    else if ( Num_electrums < sizeof(Electrums)/sizeof(*Electrums) )
        Electrums[Num_electrums++] = ep;
    else printf("Electrum server pointer buf overflow %d\n",Num_electrums);
    portable_mutex_unlock(&LP_electrummutex);
    return(ep);
}

int32_t electrum_process_array(struct iguana_info *coin,struct electrum_info *ep,char *coinaddr,cJSON *array,int32_t electrumflag)
{
    int32_t i,v,n,ht,flag = 0; char str[65]; uint64_t value; bits256 txid; cJSON *item,*retjson,*txobj; struct LP_transaction *tx;
    if ( array != 0 && coin != 0 && (n= cJSON_GetArraySize(array)) > 0 )
    {
        //printf("PROCESS %s/%s %s num.%d\n",coin->symbol,ep!=0?ep->symbol:"nanolistunspent",coinaddr,n);
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            if ( electrumflag == 0 )
            {
                txid = jbits256(item,"txid");
                v = jint(item,"vout");
                value = LP_value_extract(item,0);
                ht = LP_txheight(coin,txid);
                if ( (retjson= LP_gettxout(coin->symbol,coinaddr,txid,v)) != 0 )
                    free_json(retjson);
                else
                {
                    //printf("external unspent has no gettxout\n");
                    flag += LP_address_utxoadd("electrum process",coin,coinaddr,txid,v,value,0,1);
                }
            }
            else
            {
                txid = jbits256(item,"tx_hash");
                v = jint(item,"tx_pos");
                value = j64bits(item,"value");
                ht = jint(item,"height");
            }
            if ( bits256_nonz(txid) == 0 )
                continue;
            if ( (tx= LP_transactionfind(coin,txid)) == 0 )
            {
                txobj = LP_transactioninit(coin,txid,0,0);
                LP_transactioninit(coin,txid,1,txobj);
                free_json(txobj);
                tx = LP_transactionfind(coin,txid);
            }
            if ( tx != 0 )
            {
                if (tx->height <= 0 )
                {
                    tx->height = ht;
                    //printf("%s %s >>>>>>>>>> set %s <- height %d\n",coin->symbol,coinaddr,bits256_str(str,txid),tx->height);
                }
                if ( v >= 0 && v < tx->numvouts )
                {
                    if ( tx->outpoints[v].value == 0 && value != tx->outpoints[v].value )
                    {
                        printf("%s %s >>>>>>>>>> set %s/v%d <- %.8f vs %.8f\n",coin->symbol,coinaddr,bits256_str(str,txid),v,dstr(value),dstr(tx->outpoints[v].value));
                        tx->outpoints[v].value = value;
                    }
                }
                if ( tx->height > 0 )
                {
                    //printf("from electrum_process_array\n");
                    flag += LP_address_utxoadd("electrum process2",coin,coinaddr,txid,v,value,tx->height,-1);
                }
                //printf("v.%d numvouts.%d %.8f (%s)\n",v,tx->numvouts,dstr(tx->outpoints[jint(item,"tx_pos")].value),jprint(item,0));
            } //else printf("cant find tx\n");
        }
    }
    return(flag);
}

cJSON *electrum_version(char *symbol,struct electrum_info *ep,cJSON **retjsonp);
cJSON *electrum_headers_subscribe(char *symbol,struct electrum_info *ep,cJSON **retjsonp);

cJSON *electrum_submit(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *method,char *params,int32_t timeout)
{
    // queue id and string and callback
    char stratumreq[16384]; uint32_t expiration; struct stritem *sitem;
    if ( ep == 0 )
        ep = electrum_server(symbol,0);
    while ( ep != 0 )
    {
        if ( ep != 0 && ep->sock >= 0 && retjsonp != 0 )
        {
            *retjsonp = 0;
            sprintf(stratumreq,"{ \"jsonrpc\":\"2.0\", \"id\": %u, \"method\":\"%s\", \"params\": %s }\n",ep->stratumid,method,params);
//printf("%s %s",symbol,stratumreq);
            memset(ep->buf,0,ep->bufsize);
            sitem = (struct stritem *)queueitem(stratumreq);
            sitem->expiration = timeout;
            sitem->DL.type = ep->stratumid++;
            sitem->retptrp = (void **)retjsonp;
            portable_mutex_lock(&ep->mutex);
            queue_enqueue("sendQ",&ep->sendQ,&sitem->DL);
            expiration = (uint32_t)time(NULL) + timeout + 1;
            while ( *retjsonp == 0 && time(NULL) <= expiration )
                usleep(5000);
            portable_mutex_unlock(&ep->mutex);
            if ( *retjsonp == 0 || jobj(*retjsonp,"error") != 0 )
            {
                if ( ++ep->numerrors >= LP_ELECTRUM_MAXERRORS )
                {
                    closesocket(ep->sock), ep->sock = -1;
                    if ( (ep->sock= LP_socket(0,ep->ipaddr,ep->port)) < 0 )
                        printf("error RE-connecting to %s:%u\n",ep->ipaddr,ep->port);
                    else
                    {
                        cJSON *retjson;
                        if ( (retjson= electrum_version(ep->symbol,ep,&retjson)) != 0 )
                            printf("electrum_version %s\n",jprint(retjson,1));
                        if ( (retjson= electrum_headers_subscribe(ep->symbol,ep,&retjson)) != 0 )
                            free_json(retjson);
                        printf("ep.%p %s numerrors.%d too big -> new %s:%u sock.%d\n",ep,ep->symbol,ep->numerrors,ep->ipaddr,ep->port,ep->sock);
                        ep->numerrors = 0;
                    }
                }
            } else if ( ep->numerrors > 0 )
                ep->numerrors++;
            if ( ep->prev == 0 )
            {
                if ( *retjsonp == 0 )
                {
                    //printf("unexpected %s timeout with null retjson: %s %s\n",ep->symbol,method,params);
                    *retjsonp = cJSON_Parse("{\"error\":\"timeout\"}");
                }
                return(*retjsonp);
            }
        } else printf("couldnt find electrum server for (%s %s) or no retjsonp.%p\n",method,params,retjsonp);
        ep = ep->prev;
        //if ( ep != 0 )
        //    printf("using prev ep.%s\n",ep->symbol);
    }
    return(0);
}

cJSON *electrum_noargs(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *method,int32_t timeout)
{
    cJSON *retjson;
    if ( retjsonp == 0 )
        retjsonp = &retjson;
    return(electrum_submit(symbol,ep,retjsonp,method,"[]",timeout));
}

cJSON *electrum_strarg(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *method,char *arg,int32_t timeout)
{
    char params[16384]; cJSON *retjson;
    if ( strlen(arg) < sizeof(params) )
    {
        if ( retjsonp == 0 )
            retjsonp = &retjson;
        sprintf(params,"[\"%s\"]",arg);
        return(electrum_submit(symbol,ep,retjsonp,method,params,timeout));
    } else return(0);
}

cJSON *electrum_intarg(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *method,int32_t arg,int32_t timeout)
{
    char params[64]; cJSON *retjson;
    if ( retjsonp == 0 )
        retjsonp = &retjson;
    sprintf(params,"[\"%d\"]",arg);
    return(electrum_submit(symbol,ep,retjsonp,method,params,timeout));
}

cJSON *electrum_hasharg(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *method,bits256 arg,int32_t timeout)
{
    char params[128],str[65]; cJSON *retjson;
    if ( retjsonp == 0 )
        retjsonp = &retjson;
    sprintf(params,"[\"%s\"]",bits256_str(str,arg));
    return(electrum_submit(symbol,ep,retjsonp,method,params,timeout));
}

cJSON *electrum_version(char *symbol,struct electrum_info *ep,cJSON **retjsonp)
{
    char params[128]; cJSON *retjson;
    if ( retjsonp == 0 )
        retjsonp = &retjson;
    sprintf(params,"[\"barterDEX\", [\"1.1\", \"1.1\"]]");
    return(electrum_submit(symbol,ep,retjsonp,"server.version",params,ELECTRUM_TIMEOUT));
}


cJSON *electrum_banner(char *symbol,struct electrum_info *ep,cJSON **retjsonp) { return(electrum_noargs(symbol,ep,retjsonp,"server.banner",ELECTRUM_TIMEOUT)); }
cJSON *electrum_donation(char *symbol,struct electrum_info *ep,cJSON **retjsonp) { return(electrum_noargs(symbol,ep,retjsonp,"server.donation_address",ELECTRUM_TIMEOUT)); }
cJSON *electrum_peers(char *symbol,struct electrum_info *ep,cJSON **retjsonp) { return(electrum_noargs(symbol,ep,retjsonp,"server.peers.subscribe",ELECTRUM_TIMEOUT)); }
cJSON *electrum_features(char *symbol,struct electrum_info *ep,cJSON **retjsonp) { return(electrum_noargs(symbol,ep,retjsonp,"server.features",ELECTRUM_TIMEOUT)); }
cJSON *electrum_headers_subscribe(char *symbol,struct electrum_info *ep,cJSON **retjsonp) { return(electrum_noargs(symbol,ep,retjsonp,"blockchain.headers.subscribe",ELECTRUM_TIMEOUT)); }

cJSON *electrum_script_getbalance(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *script) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.scripthash.get_balance",script,ELECTRUM_TIMEOUT)); }
cJSON *electrum_script_gethistory(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *script) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.scripthash.get_history",script,ELECTRUM_TIMEOUT)); }
cJSON *electrum_script_getmempool(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *script) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.scripthash.get_mempool",script,ELECTRUM_TIMEOUT)); }
cJSON *electrum_script_listunspent(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *script) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.scripthash.listunspent",script,ELECTRUM_TIMEOUT)); }
cJSON *electrum_script_subscribe(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *script) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.scripthash.subscribe",script,ELECTRUM_TIMEOUT)); }

cJSON *electrum_address_subscribe(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *addr)
{
    cJSON *retjson;
    if ( (retjson= electrum_strarg(symbol,ep,retjsonp,"blockchain.address.subscribe",addr,ELECTRUM_TIMEOUT)) != 0 )
    {
        printf("subscribe.(%s)\n",jprint(retjson,0));
    }
    return(retjson);
}

cJSON *electrum_address_gethistory(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *addr)
{
    char str[65]; struct LP_transaction *tx; cJSON *retjson,*txobj,*item; int32_t i,n,height; bits256 txid; struct iguana_info *coin = LP_coinfind(symbol);
    retjson = electrum_strarg(symbol,ep,retjsonp,"blockchain.address.get_history",addr,ELECTRUM_TIMEOUT);
    //printf("history.(%s)\n",jprint(retjson,0));
    if ( retjson != 0 && (n= cJSON_GetArraySize(retjson)) > 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(retjson,i);
            txid = jbits256(item,"tx_hash");
            height = jint(item,"height");
            if ( (tx= LP_transactionfind(coin,txid)) == 0 )
            {
                //char str[65]; printf("history txinit %s ht.%d\n",bits256_str(str,txid),height);
                txobj = LP_transactioninit(coin,txid,0,0);
                txobj = LP_transactioninit(coin,txid,1,txobj);
                if ( txobj != 0 )
                    free_json(txobj);
                if ( height > 0 )
                {
                    if ( (tx= LP_transactionfind(coin,txid)) != 0 )
                    {
                        if ( tx->height > 0 && tx->height != height )
                            printf("update %s height.%d <- %d\n",bits256_str(str,txid),tx->height,height);
                        tx->height = height;
                        LP_address_utxoadd("electrum history",coin,addr,txid,0,0,height,-1);
                    }
                }
            }
        }
    }
    return(retjson);
}

int32_t LP_txheight_check(struct iguana_info *coin,char *coinaddr,struct LP_address_utxo *up)
{
    cJSON *retjson;
    if ( coin->electrum != 0 )
    {
        if ( (retjson= electrum_address_gethistory(coin->symbol,coin->electrum,&retjson,coinaddr)) != 0 )
            free_json(retjson);
    }
    return(0);
}

cJSON *electrum_address_getmempool(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *addr)
{
    cJSON *retjson; struct iguana_info *coin = LP_coinfind(symbol);
    retjson = electrum_strarg(symbol,ep,retjsonp,"blockchain.address.get_mempool",addr,ELECTRUM_TIMEOUT);
    //printf("MEMPOOL.(%s)\n",jprint(retjson,0));
    electrum_process_array(coin,ep,addr,retjson,1);
    return(retjson);
}

cJSON *electrum_address_listunspent(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *addr,int32_t electrumflag)
{
    cJSON *retjson=0; char *retstr; struct LP_address *ap; struct iguana_info *coin; int32_t updatedflag,height,usecache=1;
    if ( (coin= LP_coinfind(symbol)) == 0 )
        return(0);
    if ( ep == 0 || ep->heightp == 0 )
        height = coin->longestchain;
    else height = *(ep->heightp);
    if ( (ap= LP_address(coin,addr)) != 0 )
    {
        if ( ap->unspenttime == 0 )
            usecache = 0;
        else if ( ap->unspentheight < height )
            usecache = 0;
        else if ( G.LP_pendingswaps != 0 && time(NULL) > ap->unspenttime+20 )
            usecache = 0;
    }
    //printf("electrum.%s/%s listunspent last.(%s lag %d)\n",ep->symbol,coin->symbol,coin->lastunspent,(int32_t)(time(NULL) - coin->unspenttime));
    if ( usecache == 0 )
    {
        if ( (retjson= electrum_strarg(symbol,ep,retjsonp,"blockchain.address.listunspent",addr,ELECTRUM_TIMEOUT)) != 0 )
        {
            //printf("%s.%d u.%u/%d t.%ld %s LISTUNSPENT.(%d)\n",coin->symbol,height,ap->unspenttime,ap->unspentheight,time(NULL),addr,(int32_t)strlen(jprint(retjson,0)));
            updatedflag = 0;
            if ( electrum_process_array(coin,ep,addr,retjson,electrumflag) != 0 )
                LP_postutxos(coin->symbol,addr), updatedflag = 1;
            if ( strcmp(addr,coin->smartaddr) == 0 )
            {
                retstr = jprint(retjson,0);
                LP_unspents_cache(coin->symbol,coin->smartaddr,retstr,updatedflag);
                free(retstr);
            }
            if ( ap != 0 )
            {
                ap->unspenttime = (uint32_t)time(NULL);
                ap->unspentheight = height;
            }
        }
    } else retjson = LP_address_utxos(coin,addr,1);
    return(retjson);
}

cJSON *electrum_address_getbalance(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *addr)
{
    return(electrum_strarg(symbol,ep,retjsonp,"blockchain.address.get_balance",addr,ELECTRUM_TIMEOUT));
}

cJSON *electrum_addpeer(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *endpoint) { return(electrum_strarg(symbol,ep,retjsonp,"server.add_peer",endpoint,ELECTRUM_TIMEOUT)); }
cJSON *electrum_sendrawtransaction(char *symbol,struct electrum_info *ep,cJSON **retjsonp,char *rawtx) { return(electrum_strarg(symbol,ep,retjsonp,"blockchain.transaction.broadcast",rawtx,ELECTRUM_TIMEOUT)); }

cJSON *electrum_estimatefee(char *symbol,struct electrum_info *ep,cJSON **retjsonp,int32_t numblocks)
{
    return(electrum_intarg(symbol,ep,retjsonp,"blockchain.estimatefee",numblocks,ELECTRUM_TIMEOUT));
}

cJSON *electrum_getchunk(char *symbol,struct electrum_info *ep,cJSON **retjsonp,int32_t n) { return(electrum_intarg(symbol,ep,retjsonp,"blockchain.block.get_chunk",n,ELECTRUM_TIMEOUT)); }

cJSON *electrum_getheader(char *symbol,struct electrum_info *ep,cJSON **retjsonp,int32_t n)
{
    return(electrum_intarg(symbol,ep,retjsonp,"blockchain.block.get_header",n,ELECTRUM_TIMEOUT));
}

cJSON *LP_transaction_fromdata(struct iguana_info *coin,bits256 txid,uint8_t *serialized,int32_t len)
{
    uint8_t *extraspace; cJSON *txobj; char str[65],str2[65]; struct iguana_msgtx msgtx; bits256 checktxid;
    extraspace = calloc(1,4000000);
    memset(&msgtx,0,sizeof(msgtx));
    txobj = bitcoin_data2json(coin->taddr,coin->pubtype,coin->p2shtype,coin->isPoS,coin->height,&checktxid,&msgtx,extraspace,4000000,serialized,len,0,0,coin->zcash);
    //printf("TX.(%s) match.%d\n",jprint(txobj,0),bits256_cmp(txid,checktxid));
    free(extraspace);
    if ( bits256_cmp(txid,checktxid) != 0 )
    {
        printf("%s LP_transaction_fromdata mismatched txid %s vs %s\n",coin->symbol,bits256_str(str,txid),bits256_str(str2,checktxid));
        free_json(txobj);
        txobj = 0;
    }
    return(txobj);
}

cJSON *_electrum_transaction(char *symbol,struct electrum_info *ep,cJSON **retjsonp,bits256 txid)
{
    char *hexstr,str[65]; int32_t len; cJSON *hexjson,*txobj=0; struct iguana_info *coin; uint8_t *serialized; struct LP_transaction *tx;
    //printf("electrum_transaction %s %s\n",symbol,bits256_str(str,txid));
    if ( bits256_nonz(txid) != 0 && (coin= LP_coinfind(symbol)) != 0 )
    {
        if ( (tx= LP_transactionfind(coin,txid)) != 0 && tx->serialized != 0 )
        {
            //char str[65]; printf("%s cache hit -> TRANSACTION.(%s)\n",symbol,bits256_str(str,txid));
            if ( (txobj= LP_transaction_fromdata(coin,txid,tx->serialized,tx->len)) != 0 )
            {
                *retjsonp = txobj;
                return(txobj);
            }
        }
        if ( bits256_cmp(txid,coin->cachedtxid) == 0 )
        {
            if ( (txobj= LP_transaction_fromdata(coin,txid,coin->cachedtxiddata,coin->cachedtxidlen)) != 0 )
            {
                *retjsonp = txobj;
                return(txobj);
            }
        }
        hexjson = electrum_hasharg(symbol,ep,&hexjson,"blockchain.transaction.get",txid,ELECTRUM_TIMEOUT);
        hexstr = jprint(hexjson,0);
        if ( strlen(hexstr) > 60000 )
        {
            static uint32_t counter;
            if ( counter++ < 3 )
                printf("rawtransaction too big %d\n",(int32_t)strlen(hexstr));
            free(hexstr);
            free_json(hexjson);
            *retjsonp = cJSON_Parse("{\"error\":\"transaction too big\"}");
            return(*retjsonp);
        }
        if ( hexstr[0] == '"' && hexstr[strlen(hexstr)-1] == '"' )
            hexstr[strlen(hexstr)-1] = 0;
        if ( (len= is_hexstr(hexstr+1,0)) > 2 )
        {
            len = (int32_t)strlen(hexstr+1) >> 1;
            serialized = malloc(len);
            if ( coin->cachedtxiddata != 0 )
                free(coin->cachedtxiddata);
            coin->cachedtxiddata = malloc(len);
            coin->cachedtxidlen = len;
            decode_hex(serialized,len,hexstr+1);
            memcpy(coin->cachedtxiddata,serialized,len);
            free(hexstr);
            //printf("DATA.(%s) from (%s)\n",hexstr+1,jprint(hexjson,0));
            if ( (txobj= LP_transaction_fromdata(coin,txid,serialized,len)) != 0 )
            {
                if ( (tx= LP_transactionfind(coin,txid)) == 0 || tx->serialized == 0 )
                {
                    txobj = LP_transactioninit(coin,txid,0,txobj);
                    LP_transactioninit(coin,txid,1,txobj);
                    tx = LP_transactionfind(coin,txid);
                }
                if ( tx != 0 )
                {
                    tx->serialized = serialized;
                    tx->len = len;
                }
                else
                {
                    printf("unexpected couldnt find tx %s %s\n",coin->symbol,bits256_str(str,txid));
                    free(serialized);
                }
            }
            *retjsonp = txobj;
            free_json(hexjson);
            //printf("return from electrum_transaction\n");
            return(*retjsonp);
        } else printf("%s %s non-hex tx.(%s)\n",coin->symbol,bits256_str(str,txid),jprint(hexjson,0));
        free(hexstr);
        free_json(hexjson);
    }
    *retjsonp = 0;
    return(*retjsonp);
}

cJSON *electrum_transaction(char *symbol,struct electrum_info *ep,cJSON **retjsonp,bits256 txid)
{
    cJSON *retjson;
    if ( ep != 0 )
        portable_mutex_lock(&ep->txmutex);
    retjson = _electrum_transaction(symbol,ep,retjsonp,txid);
    if ( ep != 0 )
        portable_mutex_unlock(&ep->txmutex);
    return(retjson);
}

cJSON *electrum_getmerkle(char *symbol,struct electrum_info *ep,cJSON **retjsonp,bits256 txid,int32_t height)
{
    char params[128],str[65];
    sprintf(params,"[\"%s\", %d]",bits256_str(str,txid),height);
    if ( bits256_nonz(txid) == 0 )
        return(cJSON_Parse("{\"error\":\"null txid\"}"));
    return(electrum_submit(symbol,ep,retjsonp,"blockchain.transaction.get_merkle",params,ELECTRUM_TIMEOUT));
}

void electrum_test()
{
    cJSON *retjson; bits256 hash; struct electrum_info *ep = 0; char *addr,*script,*symbol = "BTC";
    while ( Num_electrums == 0 )
    {
        sleep(1);
        printf("Num_electrums %p -> %d\n",&Num_electrums,Num_electrums);
    }
    printf("found electrum server\n");
    if ( (retjson= electrum_version(symbol,ep,0)) != 0 )
        printf("electrum_version %s\n",jprint(retjson,1));
    if ( (retjson= electrum_banner(symbol,ep,0)) != 0 )
        printf("electrum_banner %s\n",jprint(retjson,1));
    if ( (retjson= electrum_donation(symbol,ep,0)) != 0 )
        printf("electrum_donation %s\n",jprint(retjson,1));
    if ( (retjson= electrum_features(symbol,ep,0)) != 0 )
        printf("electrum_features %s\n",jprint(retjson,1));
    if ( (retjson= electrum_estimatefee(symbol,ep,0,6)) != 0 )
        printf("electrum_estimatefee %s\n",jprint(retjson,1));
    decode_hex(hash.bytes,sizeof(hash),"0000000000000000005087f8845f9ed0282559017e3c6344106de15e46c07acd");
    if ( (retjson= electrum_getheader(symbol,ep,0,3)) != 0 )
        printf("electrum_getheader %s\n",jprint(retjson,1));
    //if ( (retjson= electrum_getchunk(symbol,ep,0,3)) != 0 )
    //    printf("electrum_getchunk %s\n",jprint(retjson,1));
    decode_hex(hash.bytes,sizeof(hash),"b967a7d55889fe11e993430921574ec6379bc8ce712a652c3fcb66c6be6e925c");
    if ( (retjson= electrum_getmerkle(symbol,ep,0,hash,403000)) != 0 )
        printf("electrum_getmerkle %s\n",jprint(retjson,1));
    if ( (retjson= electrum_transaction(symbol,ep,0,hash)) != 0 )
        printf("electrum_transaction %s\n",jprint(retjson,1));
    addr = "14NeevLME8UAANiTCVNgvDrynUPk1VcQKb";
    if ( (retjson= electrum_address_gethistory(symbol,ep,0,addr)) != 0 )
        printf("electrum_address_gethistory %s\n",jprint(retjson,1));
    if ( (retjson= electrum_address_getmempool(symbol,ep,0,addr)) != 0 )
        printf("electrum_address_getmempool %s\n",jprint(retjson,1));
    if ( (retjson= electrum_address_getbalance(symbol,ep,0,addr)) != 0 )
        printf("electrum_address_getbalance %s\n",jprint(retjson,1));
    if ( (retjson= electrum_address_listunspent(symbol,ep,0,addr,1)) != 0 )
        printf("electrum_address_listunspent %s\n",jprint(retjson,1));
    if ( (retjson= electrum_addpeer(symbol,ep,0,"electrum.be:50001")) != 0 )
        printf("electrum_addpeer %s\n",jprint(retjson,1));
    if ( (retjson= electrum_sendrawtransaction(symbol,ep,0,"0100000001b7e6d69a0fd650926bd5fbe63cc8578d976c25dbdda8dd61db5e05b0de4041fe000000006b483045022100de3ae8f43a2a026bb46f6b09b890861f8aadcb16821f0b01126d70fa9ae134e4022000925a842073484f1056c7fc97399f2bbddb9beb9e49aca76835cdf6e9c91ef3012103cf5ce3233e6d6e22291ebef454edff2b37a714aed685ce94a7eb4f83d8e4254dffffffff014c4eaa0b000000001976a914b598062b55362952720718e7da584a46a27bedee88ac00000000")) != 0 )
        printf("electrum_sendrawtransaction %s\n",jprint(retjson,1));
 
    if ( 0 )
    {
        script = "76a914b598062b55362952720718e7da584a46a27bedee88ac";
        if ( (retjson= electrum_script_gethistory(symbol,ep,0,script)) != 0 )
            printf("electrum_script_gethistory %s\n",jprint(retjson,1));
        if ( (retjson= electrum_script_getmempool(symbol,ep,0,script)) != 0 )
            printf("electrum_script_getmempool %s\n",jprint(retjson,1));
        if ( (retjson= electrum_script_getbalance(symbol,ep,0,script)) != 0 )
            printf("electrum_script_getbalance %s\n",jprint(retjson,1));
        if ( (retjson= electrum_script_listunspent(symbol,ep,0,script)) != 0 )
            printf("electrum_script_listunspent %s\n",jprint(retjson,1));
        if ( (retjson= electrum_script_subscribe(symbol,ep,0,script)) != 0 )
            printf("electrum_script_subscribe %s\n",jprint(retjson,1));
    }
    if ( (retjson= electrum_headers_subscribe(symbol,ep,0)) != 0 )
        printf("electrum_headers %s\n",jprint(retjson,1));
    if ( (retjson= electrum_peers(symbol,ep,0)) != 0 )
        printf("electrum_peers %s\n",jprint(retjson,1));
    if ( (retjson= electrum_address_subscribe(symbol,ep,0,addr)) != 0 )
        printf("electrum_address_subscribe %s\n",jprint(retjson,1));
}

struct electrum_info *LP_electrum_info(int32_t *alreadyp,char *symbol,char *ipaddr,uint16_t port,int32_t bufsize)
{
    struct electrum_info *ep=0; int32_t i,sock; struct stritem *sitem; char name[512],*str = "init string";
    *alreadyp = 0;
    portable_mutex_lock(&LP_electrummutex);
    for (i=0; i<Num_electrums; i++)
    {
        ep = Electrums[i];
        //printf("i.%d %p %s %s:%u vs %s.(%s:%u)\n",i,ep,ep->symbol,ep->ipaddr,ep->port,symbol,ipaddr,port);
        if ( strcmp(ep->ipaddr,ipaddr) == 0 && ep->port == port && strcmp(ep->symbol,symbol) == 0 )
        {
            *alreadyp = 1;
            printf("%s.(%s:%u) already an electrum server\n",symbol,ipaddr,port);
            break;
        }
        ep = 0;
    }
    portable_mutex_unlock(&LP_electrummutex);
    if ( ep == 0 )
    {
        if ( (sock= LP_socket(0,ipaddr,port)) < 0 )
        {
            printf("error connecting to %s:%u\n",ipaddr,port);
            return(0);
        }
        ep = calloc(1,sizeof(*ep) + bufsize);
        portable_mutex_init(&ep->mutex);
        portable_mutex_init(&ep->txmutex);
        ep->sock = sock;
        safecopy(ep->symbol,symbol,sizeof(ep->symbol));
        safecopy(ep->ipaddr,ipaddr,sizeof(ep->ipaddr));
        ep->port = port;
        ep->bufsize = bufsize;
        ep->coin = LP_coinfind(symbol);
        ep->lasttime = (uint32_t)time(NULL);
        sprintf(name,"%s_%s_%u_electrum_sendQ",symbol,ipaddr,port);
        queue_enqueue(name,&ep->sendQ,queueitem(str));
        if ( (sitem= queue_dequeue(&ep->sendQ)) == 0 && strcmp(sitem->str,str) != 0 )
            printf("error with string sendQ sitem.%p (%s)\n",sitem,sitem==0?0:sitem->str);
        sprintf(name,"%s_%s_%u_electrum_pendingQ",symbol,ipaddr,port);
        queue_enqueue(name,&ep->pendingQ,queueitem(str));
        if ( (sitem= queue_dequeue(&ep->pendingQ)) == 0 && strcmp(sitem->str,str) != 0 )
            printf("error with string pendingQ sitem.%p (%s)\n",sitem,sitem==0?0:sitem->str);
        electrum_server(symbol,ep);
    }
    return(ep);
}

int32_t LP_recvfunc(struct electrum_info *ep,char *str,int32_t len)
{
    cJSON *strjson,*errjson,*resultjson,*paramsjson; char *method; int32_t i,n,height; uint32_t idnum=0; struct stritem *stritem; struct iguana_info *coin; struct queueitem *tmp,*item = 0;
    if ( str == 0 || len == 0 )
        return(-1);
    ep->lasttime = (uint32_t)time(NULL);
    /*if ( (strjson= cJSON_Parse(str)) == 0 )
    {
        strjson = cJSON_CreateObject();
        resitem = cJSON_CreateObject();
        jaddstr(resitem,"string",str);
        jadd(strjson,"result",resitem);
     printf("mapped.(%s) -> %s\n",str,jprint(strjson,0));
     }*/
    if ( (strjson= cJSON_Parse(str)) != 0 )
    {
        resultjson = jobj(strjson,"result");
        //printf("strjson.(%s)\n",jprint(strjson,0));
        if ( (method= jstr(strjson,"method")) != 0 )
        {
            if ( strcmp(method,"blockchain.headers.subscribe") == 0 )
            {
                //printf("%p headers.(%s)\n",strjson,jprint(strjson,0));
                if ( (paramsjson= jarray(&n,strjson,"params")) != 0 )
                {
                    for (i=0; i<n; i++)
                        resultjson = jitem(paramsjson,i);
                }
            }
            /*else if ( strcmp(method,"blockchain.address.subscribe") == 0 ) never is called
            {
                printf("recv addr subscribe.(%s)\n",jprint(resultjson,0));
                electrum_process_array(ep->coin,resultjson);
            }*/
        }
        if ( resultjson != 0 )
        {
            if ( (height= jint(resultjson,"block_height")) > 0 && ep->heightp != 0 && ep->heighttimep != 0 )
            {
                if ( height > *(ep->heightp) )
                    *(ep->heightp) = height;
                *(ep->heighttimep) = (uint32_t)time(NULL);
                if ( (coin= LP_coinfind(ep->symbol)) != 0 )
                    coin->updaterate = (uint32_t)time(NULL);
                //printf("%s ELECTRUM >>>>>>>>> set height.%d\n",ep->symbol,height);
            }
        }
        idnum = juint(strjson,"id");
        portable_mutex_lock(&ep->pendingQ.mutex);
        if ( ep->pendingQ.list != 0 )
        {
            DL_FOREACH_SAFE(ep->pendingQ.list,item,tmp)
            {
                stritem = (struct stritem *)item;
                if ( item->type == idnum )
                {
                    //printf("matched idnum.%d result.%p\n",idnum,resultjson);
                    DL_DELETE(ep->pendingQ.list,item);
                    *((cJSON **)stritem->retptrp) = (resultjson != 0 ? jduplicate(resultjson) : jduplicate(strjson));
                    resultjson = strjson = 0;
                    free(item);
                    break;
                }
                if ( stritem->expiration < ep->lasttime )
                {
                    DL_DELETE(ep->pendingQ.list,item);
                    if ( 0 )
                    {
                        printf("expired %s (%s)\n",ep->symbol,stritem->str);
                        errjson = cJSON_CreateObject();
                        jaddnum(errjson,"id",item->type);
                        jaddstr(errjson,"error","timeout");
                        *((cJSON **)stritem->retptrp) = errjson;
                    }
                    free(item);
                }
            }
        }
        portable_mutex_unlock(&ep->pendingQ.mutex);
        if ( strjson != 0 )
            free_json(strjson);
    }
    return(item != 0);
}

void LP_dedicatedloop(void *arg)
{
    struct pollfd fds; int32_t i,len,flag,timeout = 10; struct iguana_info *coin; cJSON *retjson; struct stritem *sitem; struct electrum_info *ep = arg;
    if ( (coin= LP_coinfind(ep->symbol)) != 0 )
        ep->heightp = &coin->height, ep->heighttimep = &coin->heighttime;
    sleep(2);
    if ( (retjson= electrum_version(ep->symbol,ep,&retjson)) != 0 )
        printf("electrum_version %s\n",jprint(retjson,1));
    if ( (retjson= electrum_headers_subscribe(ep->symbol,ep,&retjson)) != 0 )
        free_json(retjson);
    printf("LP_dedicatedloop ep.%p sock.%d for %s:%u num.%d %p %s ht.%d\n",ep,ep->sock,ep->ipaddr,ep->port,Num_electrums,&Num_electrums,ep->symbol,*ep->heightp);
    while ( ep->sock >= 0 )
    {
        flag = 0;
        memset(&fds,0,sizeof(fds));
        fds.fd = ep->sock;
        fds.events |= (POLLOUT | POLLIN);
        if (  poll(&fds,1,timeout) > 0 && (fds.revents & POLLOUT) != 0 && ep->pending == 0 && (sitem= queue_dequeue(&ep->sendQ)) != 0 )
        {
            ep->pending = (uint32_t)time(NULL);
            if ( LP_socketsend(ep->sock,(uint8_t *)sitem->str,(int32_t)strlen(sitem->str)) <= 0 )
            {
                printf("%s:%u is dead\n",ep->ipaddr,ep->port);
                closesocket(ep->sock);
                ep->sock = -1;
                break;
            }
            ep->keepalive = (uint32_t)time(NULL);
            if ( sitem->expiration != 0 )
                sitem->expiration += (uint32_t)time(NULL);
            else sitem->expiration = (uint32_t)time(NULL) + ELECTRUM_TIMEOUT;
            queue_enqueue("pendingQ",&ep->pendingQ,&sitem->DL);
            flag++;
        }
        if ( flag == 0 )
        {
            if ( (fds.revents & POLLIN) != 0 )
            {
                if ( (len= LP_socketrecv(ep->sock,ep->buf,ep->bufsize)) > 0 )
                {
                    ep->pending = 0;
                    LP_recvfunc(ep,(char *)ep->buf,len);
                    flag++;
                }
            }
            if ( flag == 0 )
                usleep(100000);
        }
    }
    if ( coin->electrum == ep )
    {
        coin->electrum = ep->prev;
        printf("set %s electrum to %p\n",coin->symbol,coin->electrum);
    } else printf("backup electrum server closing\n");
    printf(">>>>>>>>>> electrum close %s:%u\n",ep->ipaddr,ep->port);
    if ( Num_electrums > 0 )
    {
        portable_mutex_lock(&LP_electrummutex);
        for (i=0; i<Num_electrums; i++)
        {
            if ( Electrums[i] == ep )
            {
                Electrums[i] = Electrums[--Num_electrums];
                Electrums[Num_electrums] = 0;
                break;
            }
        }
        portable_mutex_unlock(&LP_electrummutex);
    }
    ep->sock = -1;
    //free(ep);
}

cJSON *LP_electrumserver(struct iguana_info *coin,char *ipaddr,uint16_t port)
{
    struct electrum_info *ep; int32_t already; cJSON *retjson;
    if ( ipaddr == 0 || ipaddr[0] == 0 || port == 0 )
    {
        coin->electrum = 0;
        return(cJSON_Parse("{\"result\":\"success\",\"status\":\"electrum mode disabled, now in native coin mode\"}"));
    }
    retjson = cJSON_CreateObject();
    jaddstr(retjson,"ipaddr",ipaddr);
    jaddnum(retjson,"port",port);
    if ( (ep= LP_electrum_info(&already,coin->symbol,ipaddr,port,IGUANA_MAXPACKETSIZE * 10)) == 0 )
    {
        jaddstr(retjson,"error","couldnt connect to electrum server");
        return(retjson);
    }
    if ( already == 0 )
    {
        if ( ep != 0 && OS_thread_create(malloc(sizeof(pthread_t)),NULL,(void *)LP_dedicatedloop,(void *)ep) != 0 )
        {
            printf("error launching LP_dedicatedloop %s.(%s:%u)\n",coin->symbol,ep->ipaddr,ep->port);
            jaddstr(retjson,"error","couldnt launch electrum thread");
        }
        else
        {
            printf("launched electrum.(%s:%u)\n",ep->ipaddr,ep->port);
            jaddstr(retjson,"result","success");
            ep->prev = coin->electrum;
            coin->electrum = ep;
        }
    }
    else
    {
        jaddstr(retjson,"result","success");
        jaddstr(retjson,"status","already there");
    }
    //printf("(%s)\n",jprint(retjson,0));
    return(retjson);
}

