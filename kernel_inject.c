// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, The Linux Foundation.
 * All rights reserved.
 */
#include <linux/iommu.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#define TAG "[inject]"

#define MAX_REG_CFG_NUM 20
struct inject_reg_cfg {
	uint64_t reg;
	uint64_t val;
};
struct inject_driver_priv {
	struct platform_device *pdev;

	struct inject_reg_cfg suspend_reg_cfg[MAX_REG_CFG_NUM];
	struct inject_reg_cfg resume_reg_cfg[MAX_REG_CFG_NUM];
	int suspend_reg_cfg_cnt;
	int resume_reg_cfg_cnt;

	struct dentry *root_dentry;
};
static struct inject_driver_priv *penv;

int inject_reg_write(uint64_t reg, uint64_t val)
{
    void __iomem *base;

    u32 value;
    pr_info(TAG"%s, reg=0x%x, val=0x%x\n", __func__, reg, val);

    base = ioremap(reg, 0x100);
    if (!base) {
        pr_err(TAG"register base ioremap fail\n");
        return -1;
    }

	writel(val, base);

	value = readl(base);
	pr_info(TAG"%s, read back reg=0x%x, val=0x%x\n", __func__, reg, value);

    iounmap(base);

    return 0;
}

int inject_reg_read(uint64_t reg, uint64_t len)
{
    void __iomem *base;

    u32 value;
	int i;
	int offset;

    pr_info(TAG"%s, reg=0x%x, cnt=%d\n", __func__, reg, len);

    base = ioremap(reg, len);
    if (!base) {
        pr_err(TAG"register base ioremap fail\n");
        return -1;
    }

	// writel(val, base);
	offset = 0;
	for( i=0; i<len; i++){
		value = readl(base+offset);
		pr_info(TAG"0x%x 0x%8x\n", reg+offset, value);
		offset += 4;
	}

    iounmap(base);

    return 0;
}

static ssize_t inject_debug_write(struct file *fp, const char __user *user_buf, size_t count,
                                    loff_t *off) {
    char buf[64];
    char *sptr, *token;
    unsigned int len = 0;
    char *cmd;
    uint64_t reg;
	uint64_t val;
    const char *delim = " ";
    int ret = 0;

    len = min(count, sizeof(buf) - 1);
    if (copy_from_user(buf, user_buf, len))
        return -EINVAL;

    buf[len] = '\0';
    sptr = buf;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (!sptr)
        return -EINVAL;
    cmd = token;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &reg))
        return -EINVAL;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &val))
        return -EINVAL;

    pr_info(TAG"%s:cmd=%s, reg=%x, val=0x%x\n", __func__, cmd, reg, val);
    if (strcmp(cmd, "r") == 0) {
		inject_reg_read(reg, val);
	}
    else if (strcmp(cmd, "w") == 0) {
		inject_reg_write(reg, val);
    } else {
		pr_info(TAG"%s:non support cmd %s\n", __func__, cmd);
        return -EINVAL;
    }

    if (ret)
        return ret;

    return count;
}

static int inject_debug_show(struct seq_file *s, void *data) {
    seq_puts(s, "\nUsage: echo <CMD> <REG> <VAL>> <DEBUGFS>/inject/reg\n");
    seq_puts(s, "\nExample 1: read one register\n");
    seq_puts(s, "  echo \"r 0x3451008c 1\" > /d/inject/reg\n");
    seq_puts(s, "\nExample 2: read multy registers\n");
    seq_puts(s, "  echo \"r 0x3451008c 10\" > /d/inject/reg\n");
    seq_puts(s, "\nExample 3: write register\n");
    seq_puts(s, "  echo \"w 0x3451008c 0x2\" > /d/inject/reg\n");
    seq_puts(s, "\n");
    return 0;
}

static int inject_debug_open(struct inode *inode, struct file *file) {
    return single_open(file, inject_debug_show, inode->i_private);
}

static const struct file_operations inject_debug_fops = {
    .read = seq_read,
    .write = inject_debug_write,
    .release = single_release,
    .open = inject_debug_open,
    .owner = THIS_MODULE,
    .llseek = seq_lseek,
};


static ssize_t inject_suspend_write(struct file *fp, const char __user *user_buf, size_t count,
                                    loff_t *off) {
    char buf[64];
    char *sptr, *token;
    unsigned int len = 0;
    uint64_t reg;
	uint64_t val;
    const char *delim = " ";
    int ret = 0;

    len = min(count, sizeof(buf) - 1);
    if (copy_from_user(buf, user_buf, len))
        return -EINVAL;

    buf[len] = '\0';
    sptr = buf;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &reg))
        return -EINVAL;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &val))
        return -EINVAL;

    pr_info(TAG"%s:reg=0x%8x, val=0x%x\n", __func__, reg, val);
	if( penv->suspend_reg_cfg_cnt>=MAX_REG_CFG_NUM )
		penv->suspend_reg_cfg_cnt = 0;
	penv->suspend_reg_cfg[penv->suspend_reg_cfg_cnt].reg = reg;
	penv->suspend_reg_cfg[penv->suspend_reg_cfg_cnt].val = val;
	penv->suspend_reg_cfg_cnt++;

    if (ret)
        return ret;

    return count;
}

static int inject_suspend_show(struct seq_file *s, void *data) {
	for(int i=0; i<penv->suspend_reg_cfg_cnt; i++)
	{
		seq_printf(s, "0x%8x 0x%x\n", penv->suspend_reg_cfg[i].reg, penv->suspend_reg_cfg[i].val);
	}
    return 0;
}

static int inject_suspend_open(struct inode *inode, struct file *file) {
    return single_open(file, inject_suspend_show, inode->i_private);
}

static const struct file_operations inject_suspend_fops = {
    .read = seq_read,
    .write = inject_suspend_write,
    .release = single_release,
    .open = inject_suspend_open,
    .owner = THIS_MODULE,
    .llseek = seq_lseek,
};

static ssize_t inject_resume_write(struct file *fp, const char __user *user_buf, size_t count,
                                    loff_t *off) {
     char buf[64];
    char *sptr, *token;
    unsigned int len = 0;
    uint64_t reg;
	uint64_t val;
    const char *delim = " ";
    int ret = 0;

    len = min(count, sizeof(buf) - 1);
    if (copy_from_user(buf, user_buf, len))
        return -EINVAL;

    buf[len] = '\0';
    sptr = buf;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &reg))
        return -EINVAL;

    token = strsep(&sptr, delim);
    if (!token)
        return -EINVAL;
    if (kstrtou64(token, 0, &val))
        return -EINVAL;

    pr_info(TAG"%s:reg=0x%8x, val=0x%x\n", __func__, reg, val);
	if( penv->resume_reg_cfg_cnt>=MAX_REG_CFG_NUM )
		penv->resume_reg_cfg_cnt = 0;
	penv->resume_reg_cfg[penv->resume_reg_cfg_cnt].reg = reg;
	penv->resume_reg_cfg[penv->resume_reg_cfg_cnt].val = val;
	penv->resume_reg_cfg_cnt++;

    if (ret)
        return ret;

    return count;
}

static int inject_resume_show(struct seq_file *s, void *data) {
	for(int i=0; i<penv->resume_reg_cfg_cnt; i++)
	{
		seq_printf(s, "0x%8x 0x%x\n", penv->resume_reg_cfg[i].reg, penv->resume_reg_cfg[i].val);
	}
    return 0;
}

static int inject_resume_open(struct inode *inode, struct file *file) {
    return single_open(file, inject_resume_show, inode->i_private);
}

static const struct file_operations inject_resume_fops = {
    .read = seq_read,
    .write = inject_resume_write,
    .release = single_release,
    .open = inject_resume_open,
    .owner = THIS_MODULE,
    .llseek = seq_lseek,
};

static int inject_debugfs_create(struct inject_driver_priv *priv) {
    int ret = 0;
    struct dentry *root_dentry;

    root_dentry = debugfs_create_dir("inject", NULL);
    if (IS_ERR(root_dentry)) {
        ret = PTR_ERR(root_dentry);
        pr_err("Unable to create debugfs %d\n", ret);
        goto out;
    }
    priv->root_dentry = root_dentry;
    debugfs_create_file("reg", 0600, root_dentry, priv, &inject_debug_fops);
	debugfs_create_file("suspend_reg_cfg", 0600, root_dentry, priv, &inject_suspend_fops);
	debugfs_create_file("resume_reg_cfg", 0600, root_dentry, priv, &inject_resume_fops);

out:
    return ret;
}

static void inject_debugfs_destroy(struct inject_driver_priv *priv) {
    debugfs_remove_recursive(priv->root_dentry);
}


#ifdef CONFIG_PM_SLEEP
static int inject_pm_suspend(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;
    pr_info(TAG"%s\n",__func__);
	for(int i=0; i<penv->suspend_reg_cfg_cnt; i++)
	{
		inject_reg_write(penv->suspend_reg_cfg[i].reg, penv->suspend_reg_cfg[i].val);
	}
	return ret;
}

static int inject_pm_resume(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

    pr_info(TAG"%s\n",__func__);
	for(int i=0; i<penv->resume_reg_cfg_cnt; i++)
	{
		inject_reg_write(penv->resume_reg_cfg[i].reg, penv->resume_reg_cfg[i].val);
	}
	return ret;
}

static int inject_pm_suspend_noirq(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

    pr_info(TAG"%s\n",__func__);

	return ret;
}

static int inject_pm_resume_noirq(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

    pr_info(TAG"%s\n",__func__);

	return ret;
}

static int inject_pm_runtime_suspend(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

    pr_info(TAG"%s\n",__func__);

	return ret;
}

static int inject_pm_runtime_resume(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

    pr_info(TAG"%s\n",__func__);

	return ret;
}

static int inject_pm_runtime_idle(struct device *dev)
{
	struct inject_driver_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	pm_request_autosuspend(dev);

    pr_info(TAG"%s\n",__func__);

	return ret;
}
#endif

static const struct dev_pm_ops inject_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(inject_pm_suspend,
				inject_pm_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(inject_pm_suspend_noirq,
				      inject_pm_resume_noirq)
	SET_RUNTIME_PM_OPS(inject_pm_runtime_suspend, inject_pm_runtime_resume,
			   inject_pm_runtime_idle)
};


static int inject_driver_probe(struct platform_device *pdev) {
    int ret = 0;
    // int i;
    struct device *dev = &pdev->dev;
    struct inject_driver_priv *priv;
    if (penv) {
        pr_err("inject driver is already initialized\n");
        return -EEXIST;
    }
    pr_info("%s\n",__func__);
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    dev_set_drvdata(dev, priv);
    priv->pdev = pdev;

	inject_debugfs_create(priv);
    penv = priv;
	penv->suspend_reg_cfg_cnt = 0;
	penv->resume_reg_cfg_cnt = 0;

    pr_info("inject driver probed successfully\n");
    return ret;
}

static int inject_driver_remove(struct platform_device *pdev) {
    pr_info(TAG"%s\n",__func__);
    inject_debugfs_destroy(penv);

    dev_set_drvdata(&pdev->dev, NULL);
    return 0;
}

static const struct platform_device_id inject_platform_id[] = {
	{.name = "inject",},
	{}
};
MODULE_DEVICE_TABLE(platform, inject_platform_id);


static struct platform_driver inject_driver = {
	.id_table = inject_platform_id,
	.probe  = inject_driver_probe,
	.remove = inject_driver_remove,
	// .shutdown = inject_shutdown,
	.driver = {
		.name = "inject",
		.pm = &inject_pm_ops,
	},
};

static struct platform_device *inject_pdev;
static int __init inject_driver_initialize(void)
{
	int ret = -1;
	pr_info(TAG"%s\n",__func__);
	inject_pdev = platform_device_alloc("inject", -1);
	if(!inject_pdev){
		pr_err("Failed to allocate inject device\n");
		return ret;
	}
	ret = platform_device_add(inject_pdev);
	return platform_driver_register(&inject_driver);
}

static void __exit inject_driver_exit(void)
{
	pr_info("%s\n",__func__);
	platform_driver_unregister(&inject_driver);
	platform_device_del(inject_pdev);
	inject_pdev = NULL;
}


module_init(inject_driver_initialize);
module_exit(inject_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Inject Driver For Kernel Debug");
