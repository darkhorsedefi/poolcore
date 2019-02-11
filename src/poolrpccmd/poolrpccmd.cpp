#include "asyncio/asyncio.h"
#include "asyncio/coroutine.h"
#include "asyncio/socket.h"
#include "p2p/p2p.h"
#include "p2putils/uriParse.h"
#include "poolcommon/poolapi.h"
#include <stdio.h>
#include <string.h>
#include <chrono>

// TODO: move to another place
static const int64_t COIN = 100000000;

struct PoolRpcCmdContext {
  HostAddress address;
  p2pNode *client;
  const char *methodName;
  int argc;
  char **argv;
  bool result;
};

typedef bool methodProcTy(PoolRpcCmdContext*);

struct MethodMapElement {
  const char *name;
  methodProcTy *proc;
};

bool getInfoProc(PoolRpcCmdContext *context);
bool getCurrentBlock(PoolRpcCmdContext *context);
bool getBlockTemplate(PoolRpcCmdContext *context);
bool sendProofOfWork(PoolRpcCmdContext *context);
bool getBlockByHash(PoolRpcCmdContext *context);
bool getBalance(PoolRpcCmdContext *context);
bool sendMoney(PoolRpcCmdContext *context);

bool ZGetBalance(PoolRpcCmdContext *context);
bool ZSendMoney(PoolRpcCmdContext *context);
bool listUnspent(PoolRpcCmdContext *context);
bool ZAsyncOperationStatus(PoolRpcCmdContext *context);

bool queryFoundBlocks(PoolRpcCmdContext *context);
bool queryClientInfo(PoolRpcCmdContext *context);
bool queryPoolBalance(PoolRpcCmdContext *context);
bool queryPayouts(PoolRpcCmdContext *context);
bool queryClientStats(PoolRpcCmdContext *context);
bool queryPoolStats(PoolRpcCmdContext *context);
bool updateClientInfo(PoolRpcCmdContext *context);

bool resendBrokenTx(PoolRpcCmdContext *context);
bool moveBalance(PoolRpcCmdContext *context);
bool manualPayout(PoolRpcCmdContext *context);

static MethodMapElement methodMap[] = {
  {"getInfo", getInfoProc},
  {"getCurrentBlock", getCurrentBlock},
  {"getBlockTemplate", getBlockTemplate},
  {"sendProofOfWork", sendProofOfWork},
  {"getBlockByHash", getBlockByHash},
  {"getBalance", getBalance},
  {"sendMoney", sendMoney},
  {"z_getBalance", ZGetBalance},
  {"z_sendMoney", ZSendMoney},
  {"listUnspent", listUnspent},
  {"z_asyncOperationStatus", ZAsyncOperationStatus},
  
  {"queryFoundBlocks", queryFoundBlocks},
  {"queryPoolBalance", queryPoolBalance},
  {"queryPayouts", queryPayouts},
  {"queryClientStats", queryClientStats},
  {"queryPoolStats", queryPoolStats},  
  {"queryClientInfo", queryClientInfo},
  {"updateClientInfo", updateClientInfo},
  
  {"resendBrokenTx", resendBrokenTx},
  {"moveBalance", moveBalance},
  {"manualPayout", manualPayout},  
  {0, 0}
};

static const char *unitType(UnitType type)
{
  static const char *types[] = {
    "CPU",
    "GPU",
    "ASIC",
    "OTHER"
  };
  
  return types[std::min(type, UnitType_OTHER)];
}

static const char *asyncOpState(AsyncOpState state)
{
  static const char *states[] = {
    "ready",
    "executing",
    "cancelled",
    "failed",
    "success"
  };
  
  return states[state];
}

bool getInfoProc(PoolRpcCmdContext *context)
{   
  auto info = ioGetInfo(context->client);
  if (!info)
    return false;
  printf(" * coin: %s\n", info->coin.c_str());
  return true;
}


bool getCurrentBlock(PoolRpcCmdContext *context)
{
  auto block = ioGetCurrentBlock(context->client);
  if (!block)
    return false;
  printf(" * height: %llu\n", block->height);
  printf(" * nbits: %llu\n", block->bits);  
  printf(" * hash: %s\n", !block->hash.empty() ? block->hash.c_str() : "<empty>");
  printf(" * prevhash: %s\n", !block->prevhash.empty() ? block->prevhash.c_str() : "<empty>"); 
  if (!block->hashreserved.empty())
    printf(" * hashreserved: %s\n", block->hashreserved.c_str());  
  printf(" * time: %u\n", (unsigned)block->time);
  return true;
}

bool getBlockTemplate(PoolRpcCmdContext *context)
{
  auto beginPt = std::chrono::steady_clock::now();  
  auto block = ioGetBlockTemplate(context->client);
  if (!block)
    return false;
  auto endPt = std::chrono::steady_clock::now(); 
  auto callTime = std::chrono::duration_cast<std::chrono::microseconds>(endPt-beginPt).count() / 1000.0;
  printf("getBlockTemplate call duration %.3lfms\n", callTime);
  
  printf(" * nbits: %llu\n", block->bits);  
  printf(" * prevhash: %s\n", !block->prevhash.empty() ? block->prevhash.c_str() : "<empty>");
  if (!block->hashreserved.empty())
    printf(" * hashreserved: %s\n", block->hashreserved.c_str());    
  printf(" * merkle: %s\n", !block->merkle.empty() ? block->merkle.c_str() : "<empty>");  
  printf(" * time: %u\n", (unsigned)block->time);
  printf(" * extraNonce: %u\n", (unsigned)block->extraNonce);
  if (block->equilHashK != -1)
    printf(" * equilHashK = %i\n", block->equilHashK);
  if (block->equilHashN != -1)
    printf(" * equilHashN = %i\n", block->equilHashN);
  return true;
}

bool sendProofOfWork(PoolRpcCmdContext *context)
{
  if (context->argc != 5) {
    fprintf(stderr, "Usage: sendProofOfWork <height> <time> <nonce> <extraNonce> <data>\n");
    return false;
  }

  auto result = ioSendProofOfWork(context->client,
                                  xatoi<int64_t>(context->argv[0]),
                                  xatoi<int64_t>(context->argv[1]),
                                  context->argv[2],
                                  xatoi<int64_t>(context->argv[3]),
                                  context->argv[4]);
  if (!result)
    return false;

  if (result->result == false) {
    printf(" * proofOfWork check failed\n");
  } else {
    printf(" * proofOfWork accepted!\n");
    printf("   * generated: %lli coins\n", result->generatedCoins);
  }

  return true;
}

bool getBlockByHash(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: getBlockByHash <hash>\n");
    return false;
  }

  std::vector<std::string> hashVector = {context->argv[0]};
  auto result = ioGetBlockByHash(context->client, hashVector);
  if (!result)
    return false;

  for (size_t i = 0; i < result->blocks.size(); i++) {
    printf(" * block %u\n", (unsigned)i);
    
    auto &block = result->blocks[i];
    printf("   * height: %llu\n", block->height);
    printf("   * nbits: %llu\n", block->bits);  
    printf("   * hash: %s\n", !block->hash.empty() ? block->hash.c_str() : "<empty>");
    printf("   * prevhash: %s\n", !block->prevhash.empty() ? block->prevhash.c_str() : "<empty>"); 
    if (!block->hashreserved.empty())
      printf("   * hashreserved: %s\n", block->hashreserved.c_str());  
    printf("   * merkle: %s\n", !block->merkle.empty() ? block->merkle.c_str() : "<empty>");  
    printf("   * time: %u\n", (unsigned)block->time);
    printf("   * confirmations: %i\n", (int)block->confirmations);
  }

  return true;
}

bool getBalance(PoolRpcCmdContext *context)
{
  if (context->argc != 0) {
    fprintf(stderr, "Usage: getBalance\n");
    return false;
  }

  auto result = ioGetBalance(context->client);
  if (!result)
    return false;
  
  printf(" * balance: %lli\n", result->balance);  
  printf(" * immature: %lli\n", result->immature); 
  return true;
}


bool sendMoney(PoolRpcCmdContext *context)
{
  if (context->argc != 2) {
    fprintf(stderr, "Usage: sendMoney <destination> <amount>\n");
    return false;
  }
  
  int64_t amount = atof(context->argv[1])*COIN;
  auto result = ioSendMoney(context->client, context->argv[0], amount);
  if (!result)
    return false;
  
  printf(" * send money is %s\n", result->success ? "OK" : "FAILED");
  if (result->success)
    printf(" * transaction ID: %s\n", result->txid.c_str());
  else
    printf(" * error: %s\n", result->error.c_str());
  printf(" * fee: %lli\n", result->fee);
  printf(" * remaining: %lli\n", result->remaining);
  return true;
}

bool ZGetBalance(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: z_getBalance <address>\n  address - your z-addr or t-addr\n");
    return false;
  }
  
  auto result = ioZGetBalance(context->client, context->argv[0]);
  if (!result)
    return false;
  
  if (result->balance != -1) {
    printf(" * balance of %s: %lu\n", context->argv[0], result->balance);
  } else {
    printf("<error> %s\n", result->error.c_str());
  }

  return true;
}

bool ZSendMoney(PoolRpcCmdContext *context)
{
  if (context->argc != 4) {
    fprintf(stderr, "Usage: z_sendMoney <source> <destination> <amount> <memo>\n");
    return false;
  }
  
  ZDestinationT singleDestination;
  singleDestination.address = context->argv[1];
  singleDestination.amount = xatoi<int64_t>(context->argv[2]);
  singleDestination.memo = context->argv[3];
  
  auto result = ioZSendMoney(context->client, context->argv[0], { singleDestination })  ;
  if (!result)
    return false;
  
  if (!result->asyncOperationId.empty()) {
    printf(" * async operation id: %s\n", result->asyncOperationId.c_str());
  } else {
    printf("<error> %s\n", result->error.c_str());
  }

  return true;
}

bool listUnspent(PoolRpcCmdContext *context)
{
  if (context->argc != 0) {
    fprintf(stderr, "Usage: listUnspent\n");
    return false;
  }
  
  auto result = ioListUnspent(context->client);
  if (!result)
    return false;
  
  for (size_t i = 0; i < result->outs.size(); i++) {
    printf(" * out %u\n", (unsigned)i);
    
    auto &out = result->outs[i];
    printf("   * address: %s\n", out->address.c_str());
    printf("   * amount: %lu\n", (unsigned long)out->amount);
    printf("   * confirmations: %i\n", (int)out->confirmations);
    printf("   * spendable: %s\n", out->spendable ? "true" : "false");
  }

  return true;
}

bool ZAsyncOperationStatus(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: z_asyncOperationStatus <asyncOpId>\n");
    return false;
  }
  
  auto result = ioAsyncOperationStatus(context->client, context->argv[0]);
  if (!result)
    return false;
  
  printf(" * operations number: %u\n", (unsigned)result->status.size());
  for (size_t i = 0; i < result->status.size(); i++) {
    printf(" * async operation %u\n", (unsigned)i);
    auto &status = result->status[i];
    printf("   * id: %s\n", status->id.c_str());
    printf("   * status: %s\n", asyncOpState(status->state));
    printf("   * time: %lu\n", (unsigned long)status->creationTime);
    if (!status->txid.empty())
      printf("   * txid: %s\n", status->txid.c_str());
    if (!status->error.empty())
      printf("   * error: %s\n", status->error.c_str());
  }

  return true;
}

bool queryFoundBlocks(PoolRpcCmdContext *context)
{
  if (context->argc != 3) {
    fprintf(stderr, "Usage: queryFoundBlocks <heightFrom> <hashFrom> <count>\n");
    return false;
  }

  auto result = ioQueryFoundBlocks(context->client, xatoi<int64_t>(context->argv[0]), context->argv[1], xatoi<unsigned>(context->argv[2]));
  if (!result)
    return false;
  
  for (size_t i = 0; i < result->blocks.size(); i++) {
    printf(" * block %u\n", (unsigned)i);
    
    auto &block = result->blocks[i];
    printf("   * height: %llu\n", block->height);
    printf("   * hash: %s\n", !block->hash.empty() ? block->hash.c_str() : "<empty>");
    printf("   * time: %u\n", (unsigned)block->time);
    printf("   * confirmations: %i\n", (int)block->confirmations);
    printf("   * foundBy: %s\n", !block->foundBy.empty() ? block->foundBy.c_str() : "<empty>");    
  }

  return true;
}

bool queryClientInfo(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: queryClientInfo <userId>\n");
    return false;
  }
  
  auto result = ioQueryClientInfo(context->client, context->argv[0]);
  if (!result)
    return false;
  
  auto &info = result->info;
  printf("\nbalance: %.3lf, requested: %.3lf, paid: %.3lf, name: %s, email: %s, minimalPayout: %.3lf\n",
         (double)info->balance/COIN,
         (double)info->requested/COIN,
         (double)info->paid/COIN,
         !info->name.empty() ? info->name.c_str() : "<empty>",
         !info->email.empty() ? info->email.c_str() : "<empty>",
         (double)(info->minimalPayout)/COIN);

  return true;
}

bool queryPoolBalance(PoolRpcCmdContext *context)
{
  if (context->argc != 2) {
    fprintf(stderr, "Usage: queryPoolBalance <timeFrom> <count>\n");
    return false;
  }

  auto result = ioQueryPoolBalance(context->client, xatoi<int64_t>(context->argv[0]), xatoi<unsigned>(context->argv[1]));
  if (!result)
    return false;
  
  printf("\ttime\t\t\tbalance\t\t\timmature\tusers\t\tqueued\t\tnet\n\n");
  auto recordsNum = result->poolBalances.size();
  for (decltype(recordsNum) i = 0; i < recordsNum; i++) {
    auto &pb = result->poolBalances[i];
    printf("\t%u\t\t%.3lf\t\t%.3lf\t\t%.3lf\t\t%.3lf\t\t%.3lf\n",
           (unsigned)pb->time,
           (double)pb->balance / COIN,
           (double)pb->immature / COIN,
           (double)pb->users / COIN,
           (double)pb->queued / COIN,
           (double)pb->net / COIN);
  }

  return true;
}

bool queryPayouts(PoolRpcCmdContext *context)
{
  if (context->argc != 4) {
    fprintf(stderr, "Usage: queryPayouts <userId> <groupingTy> <timeFrom> <count>\n");
    return false;
  }
  
  
  GroupByType grouping;
  if (strcmp(context->argv[1], "none") == 0) {
    grouping = GroupByType_None;
  } else if (strcmp(context->argv[1], "hour") == 0) {
    grouping = GroupByType_Hour;
  } else if (strcmp(context->argv[1], "day") == 0) {
    grouping = GroupByType_Day;
  } else if (strcmp(context->argv[1], "week") == 0) {
    grouping = GroupByType_Week;
  } else if (strcmp(context->argv[1], "month") == 0) {
    grouping = GroupByType_Month;
  } 

  auto result = ioQueryPayouts(context->client,
                               context->argv[0],
                               grouping,
                               xatoi<int64_t>(context->argv[2]),
                               xatoi<unsigned>(context->argv[3]));
  if (!result)
    return false;

  auto payoutsNum = result->payouts.size();
  for (decltype(payoutsNum) i = 0; i < payoutsNum; i++) {
    auto &record = result->payouts[i];
    printf("  %s (%u)    %.3lf    txid: %s\n",
           record->timeLabel.c_str(),
           (unsigned)record->time,
           (double)record->value / COIN,
           !record->txid.empty() ? record->txid.c_str() : "<no txid>");
  }

  return true;
}

bool queryClientStats(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: queryClientStats <userId>\n");
    return false;
  }
  
  auto result = ioQueryClientStats(context->client, context->argv[0]);
  if (!result)
    return false;
  
  auto workersNum = result->workers.size();
  for (decltype(workersNum) i = 0; i < workersNum; i++) {
    auto &worker = result->workers[i];
    printf("  * %s addr=%s; power=%u; latency: %i; type: %s; units: %u; temp: %i\n",
           worker->name.c_str(),
           worker->address.c_str(),
           (unsigned)worker->power,
           (int)worker->latency,
           unitType(worker->type),
           (unsigned)worker->units,
           (int)worker->temp);
  }
  
  printf("\nTotal:\n  workers: %u\n  cpus:  %u\n  gpus:  %u\n  asics:  %u\n  other:  %u\n  latency: %i\n  power: %u\n", 
         (unsigned)workersNum,
         (unsigned)result->aggregate->cpus,
         (unsigned)result->aggregate->gpus,
         (unsigned)result->aggregate->asics,
         (unsigned)result->aggregate->other,
         (int)result->aggregate->avgerageLatency,
         (unsigned)result->aggregate->power);
  return true;
}

bool queryPoolStats(PoolRpcCmdContext *context)
{
  if (context->argc != 0) {
    fprintf(stderr, "Usage: queryPoolStats\n");
    return false;
  }
  
  auto result = ioQueryPoolStats(context->client);
  if (!result)
    return false;
  
  printf("\nTotal:\n  clients: %u\n  workers: %u\n  cpus:  %u\n  gpus:  %u\n  asics:  %u\n  other:  %u\n  latency: %i\n  power: %u\n", 
         (unsigned)result->aggregate->clients,
         (unsigned)result->aggregate->workers,
         (unsigned)result->aggregate->cpus,
         (unsigned)result->aggregate->gpus,
         (unsigned)result->aggregate->asics,
         (unsigned)result->aggregate->other,
         (int)result->aggregate->avgerageLatency,
         (unsigned)result->aggregate->power);
  return true;
}

bool updateClientInfo(PoolRpcCmdContext *context)
{
  if (context->argc != 4) {
    fprintf(stderr, "Usage: updateClientInfo <userId> <minimalPayout> <userName> <email>\n");
    return false;
  }
  
  auto result = ioUpdateClientInfo(context->client, context->argv[0], context->argv[2], context->argv[3], xatoi<int64_t>(context->argv[1]));
  if (!result)
    return false;
  
  printf("successfully updated\n");
  return true;
}

bool resendBrokenTx(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: resendBrokenTx <userId>\n");
    return false;
  }
  
  auto result = ioResendBrokenTx(context->client, context->argv[0]);
  if (!result)
    return false;
  
  printf("successfully called\n");
  return true;
}

bool moveBalance(PoolRpcCmdContext *context)
{
  if (context->argc != 2) {
    fprintf(stderr, "Usage: moveBalance <from> <to>\n");
    return false;
  }
  
  auto result = ioMoveBalance(context->client, context->argv[0], context->argv[1]);
  if (!result)
    return false;
  
  printf(result->status == 1 ? "successfully called\n" : "moveBalance reported error");
  return true;
}

bool manualPayout(PoolRpcCmdContext *context)
{
  if (context->argc != 1) {
    fprintf(stderr, "Usage: manualPayout <address>\n\n");
    return false;
  }
  
  auto result = ioManualPayout(context->client, context->argv[0]);
  if (!result)
    return false;
  
  printf(result->status == 1 ? "successfully called\n" : "manualPayout reported error");
  return true;
}

methodProcTy *getMethodProc(const char *name)
{
  MethodMapElement *element = methodMap;
  while (element->name) {
    if (strcmp(name, element->name) == 0)
      return element->proc;
    element++;
  }
  
  
  return 0;
}

void requestProc(void *arg)
{
  PoolRpcCmdContext *context = (PoolRpcCmdContext*)arg;
  
  if (!context->client->ioWaitForConnection(3000000)) {
    fprintf(stderr, "Error: connecting error\n");
    postQuitOperation(context->client->base());
    return;
  }

  methodProcTy *proc = getMethodProc(context->methodName);
  if (!proc) {
    fprintf(stderr, "Error: invalid method name: %s\n", context->methodName);
    postQuitOperation(context->client->base());
    return;
  }
  
  context->result = proc(context);
  postQuitOperation(context->client->base());
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s <url> <command> args...\n", argv[0]);
    return 1;
  }

  PoolRpcCmdContext context;
  initializeSocketSubsystem();

  asyncBase *base = createAsyncBase(amOSDefault);

  context.methodName = argv[2];
  context.argc = argc - 3;
  context.argv = argv + 3;
  
  // TODO 127.0.0.1:12200 change to URL from command line
  URI uri;
  if (!uriParse(argv[1], &uri)) {
    fprintf(stderr, "<error> Invalid url %s\n", argv[1]);
    return 1;
  }
  
  context.address.family = AF_INET;
  context.address.ipv4 = uri.ipv4;
  context.address.port = htons(uri.port);
  
  context.client = p2pNode::createClient(base, &context.address, 1, "pool_rpc");
  coroutineTy *proc = coroutineNew(requestProc, &context, 0x10000);  
  coroutineCall(proc);
  asyncLoop(base);
  return context.result == true ? 0 : 1;
}
