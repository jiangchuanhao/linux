#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/fs.h>

struct zla_data {
	void __iomem *reg_base;
	u32 reg_size;
	dev_t dev_num; // 设备号
	struct cdev cdev; // 字符设备结构体
	struct class *cls; // 设备类
};

static int zla_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = zla_open,
};
int zla_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct zla_data *drvdata;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	// 获取第一个内存资源
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory resource\n");
		return -EINVAL;
	}

	// 映射寄存器空间
	drvdata->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drvdata->reg_base)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return PTR_ERR(drvdata->reg_base);
	}

	drvdata->reg_size = resource_size(res);
	dev_info(&pdev->dev, "Registers mapped at %p, size %u\n",
		 drvdata->reg_base, drvdata->reg_size);

	// 自动分配设备号
	if (alloc_chrdev_region(&drvdata->dev_num, 0, 1, "zla_dev") < 0) {
		dev_err(&pdev->dev, "Failed to allocate device number\n");
		return -ENODEV;
	}

	// 初始化字符设备
	cdev_init(&drvdata->cdev, &fops);
	drvdata->cdev.owner = THIS_MODULE;

	// 添加字符设备到系统
	if (cdev_add(&drvdata->cdev, drvdata->dev_num, 1)) {
		dev_err(&pdev->dev, "Failed to add cdev\n");
		unregister_chrdev_region(drvdata->dev_num, 1);
		return -EINVAL;
	}

	// 创建设备节点
	drvdata->cls = class_create("zla_class");
	device_create(drvdata->cls, NULL, drvdata->dev_num, NULL, "zla%d", 0);

	dev_info(&pdev->dev, "Char device major:%d minor:%d\n",
		 MAJOR(drvdata->dev_num), MINOR(drvdata->dev_num));
	return 0;
	platform_set_drvdata(pdev, drvdata);
	return 0;
}
int zla_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "enter remove\n");
	struct zla_data *drvdata = platform_get_drvdata(pdev);

	device_destroy(drvdata->cls, drvdata->dev_num);
	class_destroy(drvdata->cls);
	cdev_del(&drvdata->cdev);
	unregister_chrdev_region(drvdata->dev_num, 1);

	return 0;
};
static const struct of_device_id match_table[] = {
	{ .compatible = "zynq_logic_analyzer" },
	{},
};

static struct platform_driver zla_driver = {
	.driver = { .name = "zla_driver", 
              .of_match_table = match_table,
            },
	.probe = zla_probe,
  .remove = zla_remove,
};
module_platform_driver(zla_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jiangchuanhao jiangchuanhao@gmail.com");
MODULE_DESCRIPTION("ZYNQ LOGIC ANALYZER driver");