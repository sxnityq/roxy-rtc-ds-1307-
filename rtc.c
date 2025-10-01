#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/bcd.h>



#define ROXY_REG_SEC    0x00
#   define ROXY_CH_BIT          0x80
#define ROXY_REG_MIN    0x01
#define ROXY_REG_HR     0x02
/**
*   The DS1307 can be run in either 12-hour or 24-hour mode. Bit 6 of the hours register is defined as the 12-hour or
*   24-hour mode-select bit. When high, the 12-hour mode is selected. In the 12-hour mode, bit 5 is the AM/PM bit with
*   logic high being PM. In the 24-hour mode, bit 5 is the second 10-hour bit (20 to 23 hours). The hours value must be
*   re-entered whenever the 12/24-hour mode bit is changed
*/
#   define ROXY_12_SWTCH_BIT    0x40
#define ROXY_REG_DAY    0x03
#define ROXY_REG_DATE   0x04
#define ROXY_REG_MON    0x05
#define ROXY_REG_YR     0x06
#define ROXY_REG_CTRL   0x07


struct roxy_rtc_st {
    struct rtc_device *rtc;
    char *name;
    struct regmap *regmap;
};


static struct regmap_config roxy_rtc_regmap = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0xFF,
};


static struct i2c_device_id rtc_idtable[] = {
    {   
        .name="roxyrtc", 
        .driver_data=69  
    },
    {       }
};

static struct of_device_id rtc_oftable[] = {
    {   
        .compatible="roxyrtc",
        .name="roxyrtcdts1307",
    },
    {       }
};

static int roxy_read_time(struct device *dev, struct rtc_time *tm)
{
    printk("read time\n");
    struct roxy_rtc_st *st;
    int result;
    u8 regs[7];
    
    st = dev_get_drvdata(dev);
    
    if(!st){
        printk("drvdata read timer error\n");
        return -EINVAL;
    }

    result = regmap_bulk_read(st->regmap, 0, regs, 7);
    
    if(result){
        printk("regmap bulk read err %d\n", result);
        return result;
    }

    tm->tm_sec = bcd2bin(regs[ROXY_REG_SEC] & 0x7f);
    tm->tm_min = bcd2bin(regs[ROXY_REG_MIN] & 0x7f);
    
    if(regs[ROXY_REG_HR] & ROXY_12_SWTCH_BIT){
        printk("12 hour mode\n");
        tm->tm_hour = bcd2bin(regs[ROXY_REG_HR] & 0x1f);
    } else {
        printk("24 hour mode\n");
        tm->tm_hour = bcd2bin(regs[ROXY_REG_HR] & 0x3f);
    }

    tm->tm_mday = bcd2bin(regs[ROXY_REG_DATE] & 0x3f);
    tm->tm_mon = bcd2bin(regs[ROXY_REG_MON] & 0x1f) - 1;
    tm->tm_year = bcd2bin(regs[ROXY_REG_YR]) + 100;
    tm->tm_wday = bcd2bin(regs[ROXY_REG_DAY] & 0x7) - 1;
    tm->tm_yday = -1;
    tm->tm_isdst = -1;


    printk("%d/%d/%d %d:%d:%d\n", 
        tm->tm_mon, tm->tm_mday, tm->tm_year,
        tm->tm_hour, tm->tm_min, tm->tm_sec
            );
    return 0;
}

static int roxy_set_time(struct device *dev, struct rtc_time *tm)
{
    printk("roxy set time\n");

    struct roxy_rtc_st *st;
    int result;
    u8 regs[7];
    st = dev_get_drvdata(dev);

    if(!st){
        return -EIO;
    }

    regs[ROXY_REG_SEC] = bin2bcd(tm->tm_sec);
    regs[ROXY_REG_MIN] = bin2bcd(tm->tm_min);
    regs[ROXY_REG_HR] = bin2bcd(tm->tm_hour);

    regs[ROXY_REG_DAY] = bin2bcd(tm->tm_wday + 1);
    regs[ROXY_REG_DATE] = bin2bcd(tm->tm_mday);
    regs[ROXY_REG_MON] = (bin2bcd(tm->tm_mon + 1));
    regs[ROXY_REG_YR] = (bin2bcd(tm->tm_year % 100));

    result = regmap_bulk_write(st->regmap, 0, regs, 7);
    
    printk("=========SET TIME========\n"
        "%d/%d/%d %d:%d:%d\n==============\n", 
        tm->tm_mon, tm->tm_mday, tm->tm_year,
        tm->tm_hour, tm->tm_min, tm->tm_sec
    );
    if(result != 0){
        printk("regmap_bulk_write\n");
        return result;
    }
    return 0;
}


static struct rtc_class_ops roxy_rtc_ops = {
    .read_time = roxy_read_time,
    .set_time = roxy_set_time,
};


static int rtc_probe(struct i2c_client *client)
{
    const struct of_device_id *match;
    struct roxy_rtc_st *roxy_rtc_st;
    int result;

    match = of_match_device(rtc_oftable, &client->dev);
    
    if(match){
        /* device treee goes on here */
        printk("match occured: %s\n", match->name);
    } else {
        return -ENODEV;
    }

    roxy_rtc_st = devm_kzalloc(&client->dev, sizeof(struct roxy_rtc_st), GFP_KERNEL);

    if(!roxy_rtc_st){
        printk("devm_kzalloc err\n");
        return -ENOMEM;
    }

    dev_set_drvdata(&client->dev, (void *)roxy_rtc_st);
    roxy_rtc_st->regmap = devm_regmap_init_i2c(client, &roxy_rtc_regmap);

    roxy_rtc_st->rtc = devm_rtc_device_register(&client->dev, client->name, 
                            &roxy_rtc_ops, THIS_MODULE);

    if(IS_ERR(roxy_rtc_st->rtc)){
		printk("rtc erorr\n");
        return PTR_ERR(roxy_rtc_st->rtc);
    }
    
    if(IS_ERR(roxy_rtc_st->regmap)){
        printk("regmap erorr\n");
        return PTR_ERR(roxy_rtc_st->regmap);
    }

    result = regmap_write(roxy_rtc_st->regmap, ROXY_REG_HR, ROXY_12_SWTCH_BIT);
    
    if(result){
        printk("regmap write err\n");
        return result;
    }

    return 0;
}

static void rtc_remove(struct i2c_client *client)
{    
    struct roxy_rtc_st *st;

    st = (struct roxy_rtc_st *)dev_get_platdata(&client->dev);
    devm_kfree(&client->dev, st);
    
    printk("rtc_remove\n");
    return;
}

static struct i2c_driver rtc_driver = {
    .driver = {
        .name = "roxy-rtc",
        .of_match_table = rtc_oftable,
        .owner = THIS_MODULE,
    },
    .id_table = rtc_idtable,
    .probe = rtc_probe,
    .remove = rtc_remove,
};

static int  __init rtc_module_init(void)
{
    i2c_add_driver(&rtc_driver);
    return 0;
}

static void __exit rtc_module_exit(void)
{
    i2c_del_driver(&rtc_driver);
    return;
}

module_init(rtc_module_init);
module_exit(rtc_module_exit);

MODULE_DEVICE_TABLE(i2c, rtc_idtable);
MODULE_DEVICE_TABLE(of, rtc_oftable);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("R");
